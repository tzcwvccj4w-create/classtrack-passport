#import <Cocoa/Cocoa.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <fcntl.h>
#import <sys/select.h>
#import <termios.h>
#import <time.h>
#import <unistd.h>

static NSString *const CTErrorDomain = @"cn.classtrack.manager";

static NSError *CTError(NSInteger code, NSString *message)
{
    return [NSError errorWithDomain:CTErrorDomain code:code
                            userInfo:@{NSLocalizedDescriptionKey: message}];
}

static uint32_t CTCRC32(NSData *data)
{
    uint32_t crc = 0xFFFFFFFFU;
    const uint8_t *bytes = data.bytes;
    for (NSUInteger i = 0; i < data.length; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320U & (uint32_t)-(int32_t)(crc & 1U));
    }
    return ~crc;
}

static int CTOpenSerial(NSString *path, NSError **error)
{
    int fd = open(path.fileSystemRepresentation, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        if (error) *error = CTError(10, [NSString stringWithFormat:@"无法打开设备端口 %@。请确认设备已唤醒且没有被其他程序占用。", path]);
        return -1;
    }
    struct termios options;
    if (tcgetattr(fd, &options) != 0) {
        close(fd);
        if (error) *error = CTError(11, @"无法读取 USB 串口设置。");
        return -1;
    }
    cfmakeraw(&options);
    cfsetispeed(&options, B115200);
    cfsetospeed(&options, B115200);
    options.c_cflag |= CLOCAL | CREAD;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 0;
    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        close(fd);
        if (error) *error = CTError(12, @"无法配置 USB 串口。");
        return -1;
    }
    tcflush(fd, TCIOFLUSH);
    return fd;
}

static BOOL CTWriteAll(int fd, NSData *data, NSError **error)
{
    const uint8_t *bytes = data.bytes;
    NSUInteger offset = 0;
    NSTimeInterval deadline = [NSDate timeIntervalSinceReferenceDate] + 4.0;
    while (offset < data.length) {
        NSTimeInterval remaining = deadline - [NSDate timeIntervalSinceReferenceDate];
        if (remaining <= 0) {
            if (error) *error = CTError(20, @"向设备发送数据超时。");
            return NO;
        }
        fd_set writes;
        FD_ZERO(&writes);
        FD_SET(fd, &writes);
        struct timeval timeout = {(time_t)remaining, (suseconds_t)((remaining - floor(remaining)) * 1000000.0)};
        if (select(fd + 1, NULL, &writes, NULL, &timeout) <= 0) continue;
        ssize_t count = write(fd, bytes + offset, data.length - offset);
        if (count > 0) offset += (NSUInteger)count;
    }
    return YES;
}

static NSString *CTReadProtocolLine(int fd, NSError **error)
{
    NSMutableData *buffer = [NSMutableData data];
    NSTimeInterval deadline = [NSDate timeIntervalSinceReferenceDate] + 4.0;
    while ([NSDate timeIntervalSinceReferenceDate] < deadline) {
        NSTimeInterval remaining = deadline - [NSDate timeIntervalSinceReferenceDate];
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(fd, &reads);
        struct timeval timeout = {(time_t)remaining, (suseconds_t)((remaining - floor(remaining)) * 1000000.0)};
        if (select(fd + 1, &reads, NULL, NULL, &timeout) <= 0) continue;
        uint8_t chunk[1024];
        ssize_t count = read(fd, chunk, sizeof(chunk));
        if (count <= 0) continue;
        [buffer appendBytes:chunk length:(NSUInteger)count];
        const uint8_t *bytes = buffer.bytes;
        NSUInteger start = 0;
        for (NSUInteger i = 0; i < buffer.length; ++i) {
            if (bytes[i] != '\n') continue;
            NSData *lineData = [buffer subdataWithRange:NSMakeRange(start, i - start)];
            NSString *line = [[NSString alloc] initWithData:lineData encoding:NSUTF8StringEncoding];
            line = [line stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceAndNewlineCharacterSet];
            if ([line hasPrefix:@"CT "]) return [line substringFromIndex:3];
            start = i + 1;
        }
        if (start > 0) [buffer replaceBytesInRange:NSMakeRange(0, start) withBytes:NULL length:0];
    }
    if (error) *error = CTError(21, @"设备没有响应。请按一下设备按键唤醒后重试。");
    return nil;
}

static NSString *CTExchange(NSString *port, NSData *request, NSError **error)
{
    int fd = CTOpenSerial(port, error);
    if (fd < 0) return nil;
    NSString *response = nil;
    if (CTWriteAll(fd, request, error)) response = CTReadProtocolLine(fd, error);
    close(fd);
    return response;
}

static NSArray<NSString *> *CTPorts(void)
{
    NSArray<NSString *> *entries = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:@"/dev" error:nil] ?: @[];
    NSMutableArray<NSString *> *ports = [NSMutableArray array];
    for (NSString *entry in entries) {
        if ([entry hasPrefix:@"cu.usbmodem"] || [entry hasPrefix:@"cu.usbserial"])
            [ports addObject:[@"/dev" stringByAppendingPathComponent:entry]];
    }
    return [ports sortedArrayUsingSelector:@selector(compare:)];
}

static NSArray<NSArray<NSString *> *> *CTValidateClasses(NSData *data, NSError **error)
{
    if (data.length == 0 || data.length > 4096) {
        if (error) *error = CTError(30, @"课表必须是 1 到 4096 字节的 UTF-8 CSV 文件。");
        return nil;
    }
    NSString *text = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    if (!text) {
        if (error) *error = CTError(31, @"课表不是有效的 UTF-8 文件。");
        return nil;
    }
    if ([text hasPrefix:@"\uFEFF"]) text = [text substringFromIndex:1];
    NSArray<NSString *> *lines = [text componentsSeparatedByCharactersInSet:NSCharacterSet.newlineCharacterSet];
    NSMutableArray<NSString *> *nonempty = [NSMutableArray array];
    for (NSString *line in lines) if (line.length > 0) [nonempty addObject:line];
    if (nonempty.count < 2 || ![nonempty[0] isEqualToString:@"id,name,course"]) {
        if (error) *error = CTError(32, @"CSV 第一行必须是 id,name,course。");
        return nil;
    }
    if (nonempty.count - 1 > 12) {
        if (error) *error = CTError(33, @"ClassTrack 最多支持 12 个班级。");
        return nil;
    }
    NSMutableIndexSet *ids = [NSMutableIndexSet indexSet];
    NSMutableArray<NSArray<NSString *> *> *rows = [NSMutableArray array];
    for (NSUInteger index = 1; index < nonempty.count; ++index) {
        NSArray<NSString *> *fields = [nonempty[index] componentsSeparatedByString:@","];
        if (fields.count != 3) {
            if (error) *error = CTError(34, [NSString stringWithFormat:@"第 %lu 行格式不正确，字段中不能包含逗号。", (unsigned long)index + 1]);
            return nil;
        }
        NSInteger identifier = fields[0].integerValue;
        NSString *expected = [NSString stringWithFormat:@"%ld", (long)identifier];
        if (identifier < 0 || identifier > 11 || ![fields[0] isEqualToString:expected] || [ids containsIndex:(NSUInteger)identifier]) {
            if (error) *error = CTError(35, @"班级 ID 必须是 0 到 11 之间且不能重复。");
            return nil;
        }
        NSData *name = [fields[1] dataUsingEncoding:NSUTF8StringEncoding];
        NSData *course = [fields[2] dataUsingEncoding:NSUTF8StringEncoding];
        if (name.length == 0 || name.length >= 40 || course.length >= 40) {
            if (error) *error = CTError(36, @"班级或课程名称过长，或者班级名称为空。");
            return nil;
        }
        [ids addIndex:(NSUInteger)identifier];
        [rows addObject:@[fields[1], fields[2]]];
    }
    return rows;
}

@interface CTAppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property NSWindow *window;
@property NSPopUpButton *portMenu;
@property NSTextField *deviceLabel;
@property NSTextField *resultLabel;
@property NSTextView *preview;
@property NSButton *syncButton;
@property NSButton *uploadButton;
@property NSURL *csvURL;
@end

@implementation CTAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    (void)notification;
    [self buildWindow];
    self.csvURL = [[NSBundle mainBundle] URLForResource:@"classes" withExtension:@"csv"];
    [self refreshPreview];
    [self refreshPorts:nil];
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    (void)sender;
    return YES;
}

- (NSTextField *)label:(NSString *)text size:(CGFloat)size bold:(BOOL)bold
{
    NSTextField *label = [NSTextField labelWithString:text];
    label.font = bold ? [NSFont boldSystemFontOfSize:size] : [NSFont systemFontOfSize:size];
    label.lineBreakMode = NSLineBreakByWordWrapping;
    label.maximumNumberOfLines = 0;
    return label;
}

- (NSButton *)button:(NSString *)title action:(SEL)action
{
    NSButton *button = [NSButton buttonWithTitle:title target:self action:action];
    button.bezelStyle = NSBezelStyleRounded;
    button.font = [NSFont systemFontOfSize:14 weight:NSFontWeightSemibold];
    return button;
}

- (void)buildWindow
{
    NSRect frame = NSMakeRect(0, 0, 580, 570);
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                              styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable
                                                backing:NSBackingStoreBuffered defer:NO];
    self.window.title = @"ClassTrack 管理工具";
    self.window.delegate = self;
    [self.window center];

    NSStackView *root = [NSStackView stackViewWithViews:@[]];
    root.orientation = NSUserInterfaceLayoutOrientationVertical;
    root.alignment = NSLayoutAttributeLeading;
    root.spacing = 14;
    root.edgeInsets = NSEdgeInsetsMake(24, 24, 24, 24);
    root.translatesAutoresizingMaskIntoConstraints = NO;
    [self.window.contentView addSubview:root];
    [NSLayoutConstraint activateConstraints:@[
        [root.leadingAnchor constraintEqualToAnchor:self.window.contentView.leadingAnchor],
        [root.trailingAnchor constraintEqualToAnchor:self.window.contentView.trailingAnchor],
        [root.topAnchor constraintEqualToAnchor:self.window.contentView.topAnchor],
        [root.bottomAnchor constraintEqualToAnchor:self.window.contentView.bottomAnchor]
    ]];

    [root addArrangedSubview:[self label:@"ClassTrack 管理工具" size:26 bold:YES]];
    [root addArrangedSubview:[self label:@"只在窗口打开时连接设备；关闭窗口后应用完全退出。" size:13 bold:NO]];

    NSStackView *portRow = [NSStackView stackViewWithViews:@[]];
    portRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    portRow.spacing = 10;
    self.portMenu = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    [self.portMenu.widthAnchor constraintGreaterThanOrEqualToConstant:390].active = YES;
    [portRow addArrangedSubview:self.portMenu];
    [portRow addArrangedSubview:[self button:@"刷新设备" action:@selector(refreshPorts:)]];
    [root addArrangedSubview:portRow];

    self.deviceLabel = [self label:@"尚未检查设备" size:13 bold:NO];
    self.deviceLabel.textColor = [NSColor colorWithRed:0.16 green:0.42 blue:0.30 alpha:1.0];
    [root addArrangedSubview:self.deviceLabel];

    NSStackView *actions = [NSStackView stackViewWithViews:@[]];
    actions.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    actions.distribution = NSStackViewDistributionFillEqually;
    actions.spacing = 12;
    self.syncButton = [self button:@"同步 Mac 当前时间" action:@selector(syncTime:)];
    self.uploadButton = [self button:@"上传课表到设备" action:@selector(uploadClasses:)];
    [self.syncButton.heightAnchor constraintEqualToConstant:42].active = YES;
    [actions addArrangedSubview:self.syncButton];
    [actions addArrangedSubview:self.uploadButton];
    [root addArrangedSubview:actions];

    NSStackView *fileRow = [NSStackView stackViewWithViews:@[]];
    fileRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    fileRow.spacing = 10;
    [fileRow addArrangedSubview:[self label:@"当前课表" size:14 bold:YES]];
    [fileRow addArrangedSubview:[self button:@"选择 CSV…" action:@selector(chooseCSV:)]];
    [root addArrangedSubview:fileRow];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    scroll.hasVerticalScroller = YES;
    scroll.borderType = NSBezelBorder;
    self.preview = [[NSTextView alloc] initWithFrame:NSZeroRect];
    self.preview.editable = NO;
    self.preview.selectable = YES;
    self.preview.font = [NSFont systemFontOfSize:14];
    self.preview.textContainerInset = NSMakeSize(10, 10);
    scroll.documentView = self.preview;
    [scroll.heightAnchor constraintEqualToConstant:220].active = YES;
    [root addArrangedSubview:scroll];
    [scroll.widthAnchor constraintEqualToAnchor:root.widthAnchor constant:-48].active = YES;

    self.resultLabel = [self label:@"应用不会安装登录项，也不会在后台自动运行。" size:13 bold:NO];
    self.resultLabel.textColor = [NSColor colorWithRed:0.16 green:0.42 blue:0.30 alpha:1.0];
    [root addArrangedSubview:self.resultLabel];

    NSButton *quit = [self button:@"关闭应用" action:@selector(quitApp:)];
    [root addArrangedSubview:quit];
}

- (NSString *)selectedPort
{
    NSString *port = self.portMenu.titleOfSelectedItem;
    return [port hasPrefix:@"/dev/"] ? port : nil;
}

- (void)setBusy:(BOOL)busy message:(NSString *)message
{
    self.syncButton.enabled = !busy;
    self.uploadButton.enabled = !busy;
    self.resultLabel.stringValue = message;
}

- (void)runWork:(NSString *(^)(NSError **error))work
{
    [self setBusy:YES message:@"正在与设备通信…"];
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSError *error = nil;
        NSString *result = work(&error);
        dispatch_async(dispatch_get_main_queue(), ^{
            [self setBusy:NO message:result ?: [NSString stringWithFormat:@"操作失败：%@", error.localizedDescription ?: @"未知错误"]];
            if (error) {
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = @"ClassTrack 操作失败";
                alert.informativeText = error.localizedDescription ?: @"未知错误";
                [alert beginSheetModalForWindow:self.window completionHandler:nil];
            }
        });
    });
}

- (IBAction)refreshPorts:(id)sender
{
    (void)sender;
    NSString *previous = self.selectedPort;
    NSArray<NSString *> *ports = CTPorts();
    [self.portMenu removeAllItems];
    if (ports.count == 0) {
        [self.portMenu addItemWithTitle:@"未找到 USB 设备"];
        self.deviceLabel.stringValue = @"请连接数据线并按一下设备按键唤醒";
        return;
    }
    [self.portMenu addItemsWithTitles:ports];
    if (previous && [ports containsObject:previous]) [self.portMenu selectItemWithTitle:previous];
    self.deviceLabel.stringValue = ports.count == 1 ? [NSString stringWithFormat:@"已发现设备：%@", ports[0]] : @"发现多个串口，请选择 AI Passport";
}

- (IBAction)syncTime:(id)sender
{
    (void)sender;
    NSString *port = self.selectedPort;
    if (!port) { self.resultLabel.stringValue = @"未找到设备，请连接并唤醒后刷新。"; return; }
    [self runWork:^NSString *(NSError **error) {
        NSString *command = [NSString stringWithFormat:@"TIME %lld\n", (long long)time(NULL)];
        NSString *response = CTExchange(port, [command dataUsingEncoding:NSUTF8StringEncoding], error);
        if (![response isEqualToString:@"OK TIME"]) {
            if (response && error) *error = CTError(40, [NSString stringWithFormat:@"设备返回：%@", response]);
            return nil;
        }
        NSDateFormatter *formatter = [[NSDateFormatter alloc] init];
        formatter.locale = [NSLocale localeWithLocaleIdentifier:@"zh_CN"];
        formatter.timeZone = [NSTimeZone timeZoneWithName:@"Asia/Shanghai"];
        formatter.dateFormat = @"yyyy年MM月dd日 HH:mm";
        return [NSString stringWithFormat:@"时间同步成功：%@", [formatter stringFromDate:NSDate.date]];
    }];
}

- (IBAction)chooseCSV:(id)sender
{
    (void)sender;
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"csv"]];
    panel.allowsMultipleSelection = NO;
    panel.canChooseDirectories = NO;
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response == NSModalResponseOK) {
            self.csvURL = panel.URL;
            [self refreshPreview];
        }
    }];
}

- (void)refreshPreview
{
    NSData *data = self.csvURL ? [NSData dataWithContentsOfURL:self.csvURL] : nil;
    NSError *error = nil;
    NSArray<NSArray<NSString *> *> *rows = data ? CTValidateClasses(data, &error) : nil;
    if (!rows) {
        self.preview.string = error.localizedDescription ?: @"未找到课表。";
        return;
    }
    NSMutableArray<NSString *> *lines = [NSMutableArray array];
    [rows enumerateObjectsUsingBlock:^(NSArray<NSString *> *row, NSUInteger index, BOOL *stop) {
        (void)stop;
        [lines addObject:[NSString stringWithFormat:@"%lu. %@　%@", (unsigned long)index + 1, row[0], row[1]]];
    }];
    self.preview.string = [lines componentsJoinedByString:@"\n"];
    self.resultLabel.stringValue = [NSString stringWithFormat:@"已载入课表：%lu 个班级", (unsigned long)rows.count];
}

- (IBAction)uploadClasses:(id)sender
{
    (void)sender;
    NSString *port = self.selectedPort;
    if (!port) { self.resultLabel.stringValue = @"未找到设备，请连接并唤醒后刷新。"; return; }
    NSData *data = self.csvURL ? [NSData dataWithContentsOfURL:self.csvURL] : nil;
    NSError *validationError = nil;
    NSArray *rows = data ? CTValidateClasses(data, &validationError) : nil;
    if (!rows) { self.resultLabel.stringValue = validationError.localizedDescription ?: @"课表无法读取。"; return; }
    [self runWork:^NSString *(NSError **error) {
        NSString *header = [NSString stringWithFormat:@"IMPORT %lu %08x\n", (unsigned long)data.length, CTCRC32(data)];
        NSMutableData *request = [[header dataUsingEncoding:NSUTF8StringEncoding] mutableCopy];
        [request appendData:data];
        NSString *response = CTExchange(port, request, error);
        if (![response isEqualToString:@"OK IMPORT"]) {
            if (response && error) *error = CTError(41, [NSString stringWithFormat:@"设备返回：%@", response]);
            return nil;
        }
        return [NSString stringWithFormat:@"课表上传成功：共 %lu 个班级，设备首页已刷新。", (unsigned long)rows.count];
    }];
}

- (IBAction)quitApp:(id)sender
{
    (void)sender;
    [NSApp terminate:nil];
}

@end

static int CTSelfTest(void)
{
    NSData *sample = [@"123456789" dataUsingEncoding:NSUTF8StringEncoding];
    if (CTCRC32(sample) != 0xCBF43926U) return 1;
    NSString *csv = @"id,name,course\n0,26机电2班,高职英语\n";
    NSError *error = nil;
    NSArray *rows = CTValidateClasses([csv dataUsingEncoding:NSUTF8StringEncoding], &error);
    if (rows.count != 1 || error) return 2;
    fprintf(stdout, "PASS: native ClassTrack manager self-test\n");
    return 0;
}

int main(int argc, const char *argv[])
{
    @autoreleasepool {
        if (argc == 2 && strcmp(argv[1], "--self-test") == 0) return CTSelfTest();
        NSApplication *application = [NSApplication sharedApplication];
        application.activationPolicy = NSApplicationActivationPolicyRegular;
        CTAppDelegate *delegate = [[CTAppDelegate alloc] init];
        application.delegate = delegate;
        [application run];
    }
    return 0;
}

