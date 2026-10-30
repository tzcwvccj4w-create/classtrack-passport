const { chromium } = require("playwright");
const path = require("path");
const fs = require("fs");

const root = path.resolve(__dirname, "..");
const output = path.join(root, "screenshots");
const screens = ["home", "detail", "recording", "review", "recent", "settings", "timeSync"];

(async () => {
  fs.mkdirSync(output, { recursive: true });
  const browser = await chromium.launch({
    headless: true,
    executablePath: process.env.CHROME_PATH || "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
  });
  const page = await browser.newPage({ viewport: { width: 1080, height: 760 }, deviceScaleFactor: 2 });
  const failures = [];

  for (const name of screens) {
    const url = `file://${path.join(root, "index.html")}?screen=${name}`;
    await page.goto(url);
    await page.waitForTimeout(120);
    const result = await page.evaluate(() => {
      const screen = document.querySelector(".screen").getBoundingClientRect();
      const viewport = document.querySelector(".viewport").getBoundingClientRect();
      const footer = document.querySelector(".key-hint").getBoundingClientRect();
      const items = [...document.querySelectorAll("[data-layout-item]")].map(el => ({
        id: el.dataset.layoutItem,
        rect: el.getBoundingClientRect(),
        scrollWidth: el.scrollWidth,
        clientWidth: el.clientWidth,
        scrollHeight: el.scrollHeight,
        clientHeight: el.clientHeight
      }));
      const issues = [];
      if (Math.round(screen.width) !== 240 || Math.round(screen.height) !== 320) issues.push(`screen=${screen.width}x${screen.height}`);
      for (const item of items) {
        const r = item.rect;
        if (r.left < viewport.left - .5 || r.right > viewport.right + .5 || r.top < viewport.top - .5 || r.bottom > viewport.bottom + .5) issues.push(`${item.id}: outside safe viewport`);
        if (r.bottom > footer.top + .5) issues.push(`${item.id}: overlaps fixed footer`);
        if (item.scrollHeight > item.clientHeight + 1) issues.push(`${item.id}: vertical overflow`);
      }
      for (let i = 0; i < items.length; i++) for (let j = i + 1; j < items.length; j++) {
        const a = items[i].rect, b = items[j].rect;
        const overlap = Math.min(a.right,b.right)-Math.max(a.left,b.left) > 1 && Math.min(a.bottom,b.bottom)-Math.max(a.top,b.top) > 1;
        if (overlap) issues.push(`${items[i].id} overlaps ${items[j].id}`);
      }
      return { issues, size: [screen.width, screen.height] };
    });
    if (result.issues.length) failures.push({ name, issues: result.issues });
    await page.locator(".screen").screenshot({ path: path.join(output, `${name}.png`) });
  }
  await browser.close();
  if (failures.length) {
    console.error(JSON.stringify(failures, null, 2));
    process.exit(1);
  }
  console.log(`PASS: ${screens.length} screens fit 240x320 safe areas with no measured overlap.`);
})();
