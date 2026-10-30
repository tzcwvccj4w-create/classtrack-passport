const root = document.documentElement;
const languageButton = document.querySelector('[data-language-toggle]');
const header = document.querySelector('[data-header]');
const toast = document.querySelector('[data-toast]');

function setLanguage(language) {
  const next = language === 'en' ? 'en' : 'zh';
  root.dataset.language = next;
  root.lang = next === 'en' ? 'en' : 'zh-CN';
  languageButton.textContent = next === 'en' ? '中' : 'EN';
  languageButton.setAttribute('aria-label', next === 'en' ? '切换到中文' : 'Switch to English');
  document.title = next === 'en' ? 'ClassTrack · Where did the class get to?' : 'ClassTrack · 课到哪了？';
  try { localStorage.setItem('classtrack-language', next); } catch (_) {}
}

let storedLanguage = null;
try { storedLanguage = localStorage.getItem('classtrack-language'); } catch (_) {}
if (storedLanguage) setLanguage(storedLanguage);

languageButton.addEventListener('click', () => {
  setLanguage(root.dataset.language === 'zh' ? 'en' : 'zh');
});

window.addEventListener('scroll', () => {
  header.classList.toggle('scrolled', window.scrollY > 20);
}, { passive: true });

const observer = new IntersectionObserver((entries) => {
  entries.forEach((entry) => {
    if (entry.isIntersecting) {
      entry.target.classList.add('visible');
      observer.unobserve(entry.target);
    }
  });
}, { threshold: 0.12, rootMargin: '0px 0px -40px' });

document.querySelectorAll('.reveal').forEach((element) => observer.observe(element));

function showToast(message) {
  toast.textContent = message;
  toast.classList.add('visible');
  window.setTimeout(() => toast.classList.remove('visible'), 1800);
}

document.querySelector('[data-copy-csv]').addEventListener('click', async () => {
  const csv = document.querySelector('[data-csv]').textContent;
  try {
    await navigator.clipboard.writeText(csv);
    showToast(root.dataset.language === 'en' ? 'CSV copied' : '已复制 CSV');
  } catch (_) {
    showToast(root.dataset.language === 'en' ? 'Select the text to copy' : '请选中文本复制');
  }
});
