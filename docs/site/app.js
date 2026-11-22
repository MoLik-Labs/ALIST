const menu = document.querySelector('.menu-toggle');
const sidebar = document.querySelector('#sidebar');
const search = document.querySelector('#nav-search');
const links = [...document.querySelectorAll('#nav a')];
const empty = document.querySelector('#no-results');

function closeMenu() {
  sidebar.classList.remove('open');
  menu.setAttribute('aria-expanded', 'false');
}
menu.addEventListener('click', () => {
  const open = sidebar.classList.toggle('open');
  menu.setAttribute('aria-expanded', String(open));
});
links.forEach(link => link.addEventListener('click', () => {
  links.forEach(item => item.classList.remove('active'));
  link.classList.add('active');
  closeMenu();
}));
search.addEventListener('input', () => {
  const term = search.value.trim().toLowerCase();
  links.forEach(link => { link.hidden = !link.textContent.toLowerCase().includes(term); });
  document.querySelectorAll('.nav-group').forEach(group => {
    group.hidden = ![...group.querySelectorAll('a')].some(link => !link.hidden);
  });
  empty.hidden = links.some(link => !link.hidden);
});
document.addEventListener('keydown', event => {
  if (event.key === 'Escape') closeMenu();
  if (event.key === '/' && !event.ctrlKey && !event.metaKey && !event.altKey && !/^(INPUT|TEXTAREA|SELECT)$/.test(document.activeElement.tagName)) {
    event.preventDefault();
    if (window.matchMedia('(max-width: 700px)').matches) {
      sidebar.classList.add('open');
      menu.setAttribute('aria-expanded', 'true');
    }
    search.focus();
  }
});
document.querySelectorAll('[data-copy]').forEach(button => {
  button.addEventListener('click', async () => {
    try {
      await navigator.clipboard.writeText(document.getElementById(button.dataset.copy).textContent);
      button.textContent = 'Copied';
    } catch {
      button.textContent = 'Copy unavailable';
    }
    setTimeout(() => { button.textContent = 'Copy'; }, 2000);
  });
});
