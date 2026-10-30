(function () {
  "use strict";

  const classes = [
    { id: "mech-2", name: "26机电2班", course: "高职英语", recorded: true, stamp: "10月26日 10:20", duration: "00:13" },
    { id: "mech-3", name: "26机电3班", course: "高职英语", recorded: false, stamp: "尚未记录", duration: "--:--" },
    { id: "degree-1", name: "26学历1班", course: "汉语听力", recorded: true, stamp: "10月25日 15:40", duration: "00:18" },
    { id: "lang-1", name: "26语言1班", course: "汉语听力", recorded: false, stamp: "尚未记录", duration: "--:--" },
    { id: "lang-2", name: "26语言2班", course: "汉语听力", recorded: true, stamp: "10月24日 09:05", duration: "00:12" },
    { id: "lang-3", name: "26语言3班", course: "汉语听力", recorded: false, stamp: "尚未记录", duration: "--:--" }
  ];

  const state = { screen: "home", selected: 0, classIndex: 0, recordingStart: 0, timer: null, elapsed: 6, toastTimer: null };
  const viewport = document.getElementById("viewport");
  const screen = document.getElementById("screen");
  const keyHint = document.getElementById("keyHint");
  const statusDate = document.getElementById("statusDate");
  const statusTime = document.getElementById("statusTime");

  const screens = {
    home: {
      hint: "▲▼ 选择　OK 进入",
      count: () => classes.length + 2,
      render() {
        const items = classes.map((item, i) => [i, item.name, item.recorded ? "●" : "○", item.recorded ? "已记录" : "未记录"]);
        items.push([classes.length, "最近更新", "›", "按时间查看"]);
        items.push([classes.length + 1, "设置", "›", "校时 / 导入 / 备份"]);
        let start = state.selected >= 5 ? state.selected - 4 : 0;
        if (start + 7 > items.length) start = Math.max(0, items.length - 7);
        const rows = items.slice(start, start + 7).map(item => row(...item));
        return `<div class="page home"><p class="eyebrow">CLASS TRACK</p><h2 class="page-title">课到哪了？</h2><div class="list">${rows.join("")}</div></div>`;
      },
      ok() {
        if (state.selected < classes.length) { state.classIndex = state.selected; open("detail", 0); }
        else if (state.selected === classes.length) open("recent", 0);
        else open("settings", 0);
      }
    },
    detail: {
      hint: "▲ 返回　▼ 播放　OK 录音",
      count: () => 1,
      render() {
        const item = classes[state.classIndex];
        const stamp = item.recorded ? item.stamp.replace(" ", "<br>") : "尚未记录";
        return `<div class="page detail"><p class="eyebrow">${esc(item.course)}</p><h2 class="page-title">${esc(item.name)}</h2><div class="card"><div class="sub-title">最近一次记录</div><div class="record-date">${stamp}</div><div class="duration">${item.duration}</div><div class="action-copy">${item.recorded ? "按下键播放原录音" : "这个班还没有录音"}<br>按 OK 开始录音</div></div></div>`;
      },
      up() { open("home", state.classIndex); },
      down() { toast(classes[state.classIndex].recorded ? "正在播放最近一条进度录音" : "这个班还没有录音"); },
      ok() { startRecording(); }
    },
    recording: {
      hint: "OK 结束　最长 20 秒",
      count: () => 1,
      render() {
        return `<div class="page recording"><p class="eyebrow">RECORDING</p><h2 class="page-title">正在录音</h2><div class="mic">●</div><div id="recordTime" class="time-large">00:00</div><div class="meter"><i></i><i></i><i></i><i></i><i></i><i></i></div><p class="sub-title">再按 OK 结束录音<br>最长 20 秒</p></div>`;
      },
      ok() { finishRecording(); }
    },
    review: {
      hint: "▲▼ 选择　OK 确认",
      count: () => 4,
      render() {
        const actions = [
          ["▶", "试听这次录音"], ["✓", "确认保存"], ["↻", "重新录制"], ["×", "放弃修改"]
        ].map((item, i) => `<div class="review-action${i === state.selected ? " selected" : ""}" data-layout-item="review-${i}"><span>${item[0]}</span><span>${item[1]}</span></div>`).join("");
        return `<div class="page review"><p class="eyebrow">REVIEW BEFORE SAVING</p><h2 class="page-title">录音完成</h2><div class="time-large">${fmt(state.elapsed)}</div><div class="review-actions">${actions}</div><p class="review-note" data-layout-item="review-note">原录音会保留到“确认保存”完成。<br>当前为模拟录音，无真实音频。</p></div>`;
      },
      ok() {
        if (state.selected === 0) toast("模拟试听：当前没有真实音频");
        if (state.selected === 1) {
          const item = classes[state.classIndex]; item.recorded = true; item.stamp = "10月26日 21:24"; item.duration = fmt(state.elapsed); open("detail", 0); toast("已安全保存，新录音现在成为最近记录");
        }
        if (state.selected === 2) startRecording();
        if (state.selected === 3) { open("detail", 0); toast("已放弃修改，原录音未受影响"); }
      }
    },
    recent: {
      hint: "▲ 返回　▼ 浏览",
      count: () => classes.filter(c => c.recorded).length,
      render() {
        const rows = classes.filter(c => c.recorded).map((item, i) => `<div class="row${i === state.selected ? " selected" : ""}" data-layout-item="recent-${i}"><span>${i + 1}</span><span class="name">${esc(item.name)}</span><span class="meta">${item.stamp.replace(" ", "<br>")}</span></div>`);
        return `<div class="page recent"><p class="eyebrow">RECENT UPDATES</p><h2 class="page-title">最近更新</h2><div class="list">${rows.join("")}</div></div>`;
      },
      up() { open("home", classes.length); },
      ok() {
        const recorded = classes.filter(c => c.recorded); state.classIndex = classes.indexOf(recorded[state.selected]); open("detail", 0);
      }
    },
    settings: {
      hint: "▲▼ 选择　OK 执行",
      count: () => 4,
      render() {
        const items = [["◷", "Mac USB 校时", "连接工具后自动返回"], ["＋", "导入班级 CSV", "保留相同 ID 的录音"], ["⇩", "备份数据", "班级、时间戳和 ADPCM"], ["←", "返回首页", ""]];
        const rows = items.map((item, i) => `<div class="row${i === state.selected ? " selected" : ""}" data-layout-item="setting-${i}"><span>${item[0]}</span><span class="name">${item[1]}<small class="detail">${item[2]}</small></span></div>`);
        return `<div class="page settings"><p class="eyebrow">OFFLINE SETTINGS</p><h2 class="page-title">设置</h2><div class="list">${rows.join("")}</div></div>`;
      },
      up() { move(-1); }, down() { move(1); },
      ok() {
        if (state.selected === 0) open("timeSync", 0);
        if (state.selected === 1) toast("已校验并导入 6 个班级");
        if (state.selected === 2) toast("备份模拟完成：6 个班级，3 条录音");
        if (state.selected === 3) open("home", classes.length + 1);
      }
    },
    timeSync: {
      hint: "▲ 返回",
      count: () => 1,
      render() {
        return `<div class="page detail"><p class="eyebrow">USB TIME SYNC</p><h2 class="page-title">等待 Mac 校时</h2><div class="card"><div class="action-copy">请在 Mac 运行校时工具<br>同步后自动返回</div></div></div>`;
      },
      up() { open("settings", 0); },
      ok() { open("settings", 0); }
    }
  };

  function esc(text) { return String(text).replace(/[&<>"']/g, c => ({ "&":"&amp;", "<":"&lt;", ">":"&gt;", "\"":"&quot;", "'":"&#39;" })[c]); }
  function fmt(seconds) { return `00:${String(Math.max(0, Math.min(20, seconds))).padStart(2, "0")}`; }
  function row(index, name, symbol, meta) { return `<div class="row${index === state.selected ? " selected" : ""}" data-layout-item="row-${index}"><span class="dot">${symbol}</span><span class="name">${esc(name)}</span><span class="meta">${esc(meta)}</span></div>`; }

  function render() {
    screen.dataset.screen = state.screen;
    keyHint.textContent = screens[state.screen].hint;
    viewport.innerHTML = screens[state.screen].render();
  }

  function open(name, selected) { state.screen = name; state.selected = selected || 0; stopTimer(); render(); }
  function move(delta) { const n = screens[state.screen].count(); state.selected = (state.selected + delta + n) % n; render(); }
  function press(key) {
    const def = screens[state.screen];
    if (def[key]) { def[key](); return; }
    if (key === "up") move(-1); else if (key === "down") move(1); else if (key === "ok" && def.ok) def.ok();
  }

  function startRecording() {
    state.screen = "recording"; state.selected = 0; state.recordingStart = Date.now(); state.elapsed = 0; render();
    stopTimer(); state.timer = setInterval(() => {
      state.elapsed = Math.min(20, Math.max(0, Math.floor((Date.now() - state.recordingStart) / 1000)));
      const label = document.getElementById("recordTime"); if (label) label.textContent = fmt(state.elapsed);
      if (state.elapsed >= 20) finishRecording();
    }, 200);
  }
  function finishRecording() { if (state.screen !== "recording") return; stopTimer(); state.elapsed = Math.max(1, state.elapsed || 6); open("review", 0); }
  function stopTimer() { if (state.timer) clearInterval(state.timer); state.timer = null; }

  function toast(message) {
    document.querySelector(".toast")?.remove();
    const node = document.createElement("div"); node.className = "toast"; node.textContent = message; screen.appendChild(node);
    clearTimeout(state.toastTimer); state.toastTimer = setTimeout(() => node.remove(), 1800);
  }

  document.querySelectorAll("[data-key]").forEach(button => {
    const key = button.dataset.key;
    button.addEventListener("pointerdown", () => {
      button.classList.add("pressed");
    });
    button.addEventListener("pointerup", () => {
      button.classList.remove("pressed");
      press(key);
    });
    button.addEventListener("pointercancel", () => { button.classList.remove("pressed"); });
  });

  document.addEventListener("keydown", event => {
    if (event.repeat) return;
    if (event.key === "ArrowUp") press("up");
    else if (event.key === "ArrowDown") press("down");
    else if (event.key === "Enter") press("ok");
  });

  document.getElementById("resetDemo").addEventListener("click", () => { open("home", 0); toast("已模拟重启，数据仍保留"); });
  document.getElementById("syncClock").addEventListener("click", () => { open("settings", 0); toast("时间已与 Mac 同步并立即刷新"); });
  document.getElementById("importClasses").addEventListener("click", () => toast("已导入示例班级，现有录音按 ID 保留"));

  const forced = new URLSearchParams(location.search).get("screen");
  if (forced && screens[forced]) { state.screen = forced; state.selected = 0; if (forced === "review") state.elapsed = 6; }
  render();
  function updateClock() {
    const d = new Date();
    statusDate.textContent = `${d.getFullYear()}年${String(d.getMonth() + 1).padStart(2,"0")}月${String(d.getDate()).padStart(2,"0")}日`;
    statusTime.textContent = `${String(d.getHours()).padStart(2,"0")}:${String(d.getMinutes()).padStart(2,"0")}`;
  }
  updateClock();
  setInterval(updateClock, 30000);

  window.ClassTrackPrototype = { state, screens, open, render, startRecording, finishRecording };
})();
