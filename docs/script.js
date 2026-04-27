'use strict';

// ══════════════════════════════════════════════
// УТИЛИТЫ
// ══════════════════════════════════════════════
function esc(str) {
  if (str === null || str === undefined) return '—';
  return String(str)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#039;');
}

function getInitials(name) {
  if (!name) return '??';
  const parts = String(name).trim().split(/\s+/);
  return parts.length >= 2
    ? (parts[0][0] + parts[1][0]).toUpperCase()
    : parts[0].slice(0, 2).toUpperCase();
}

function memberWord(n) {
  if (n % 10 === 1 && n % 100 !== 11) return 'участник';
  if ([2,3,4].includes(n % 10) && ![12,13,14].includes(n % 100)) return 'участника';
  return 'участников';
}

// ══════════════════════════════════════════════
// CURSOR
// ══════════════════════════════════════════════
const curDot  = document.getElementById('curDot');
const curRing = document.getElementById('curRing');
let mx = 0, my = 0, rx = 0, ry = 0;
document.addEventListener('mousemove', e => { mx = e.clientX; my = e.clientY; });
(function tick() {
  if (curDot && curRing) {
    curDot.style.cssText  = `left:${mx}px;top:${my}px`;
    rx += (mx - rx) * .12; ry += (my - ry) * .12;
    curRing.style.cssText = `left:${rx}px;top:${ry}px`;
  }
  requestAnimationFrame(tick);
})();

// ══════════════════════════════════════════════
// NAV / PAGES
// ══════════════════════════════════════════════
window.addEventListener('scroll', () => {
  document.querySelector('nav')?.classList.toggle('scrolled', window.scrollY > 10);
}, { passive: true });

function showPage(id, el) {
  document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.nav-center a').forEach(a => a.classList.remove('active'));
  document.getElementById(id)?.classList.add('active');
  el?.classList.add('active');
  window.scrollTo({ top: 0, behavior: 'instant' });
  setTimeout(initReveal, 60);
  if (id === 'rooms') loadRooms();
}

// ══════════════════════════════════════════════
// SCROLL REVEAL
// ══════════════════════════════════════════════
let revObs;
function initReveal() {
  revObs?.disconnect();
  revObs = new IntersectionObserver(
    entries => entries.forEach(e => { if (e.isIntersecting) e.target.classList.add('in'); }),
    { threshold: 0.1 }
  );
  document.querySelectorAll('.reveal:not(.in)').forEach(el => revObs.observe(el));
}
initReveal();

// ══════════════════════════════════════════════
// RENDER STATS & TRUST (из data.js)
// ══════════════════════════════════════════════
function renderStats() {
  const wrap = document.getElementById('statsRow');
  if (!wrap || typeof STATS === 'undefined') return;
  wrap.innerHTML = STATS.map(s => `
    <div class="stat reveal">
      <div class="stat-num">${esc(s.value)}</div>
      <div class="stat-label">${esc(s.label)}</div>
    </div>`).join('');
}

function renderTrust() {
  const wrap = document.getElementById('trustGrid');
  if (!wrap || typeof ORGANIZATIONS === 'undefined') return;
  wrap.innerHTML = ORGANIZATIONS.map(o =>
    `<div class="trust-item"><div class="trust-name">${esc(o.name)}</div></div>`
  ).join('');
}

// ══════════════════════════════════════════════
// ROOMS — загрузка с сервера
// ══════════════════════════════════════════════
async function loadRooms() {
  const grid    = document.getElementById('roomsGrid');
  const counter = document.getElementById('roomsCount');
  if (!grid) return;

  grid.innerHTML = '<div class="rooms-loading">Загрузка...</div>';

  try {
    const res  = await fetch(`${SERVER_URL}/api/rooms`);
    if (!res.ok) throw new Error('Server error');
    const rooms = await res.json();

    if (counter) counter.textContent = rooms.length + ' комнат доступно';

    grid.innerHTML = rooms.map((r, i) => {
      const word = memberWord(r.online || 0);
      const tag  = r.open
        ? '<div class="room-tag room-tag-open">Открыто</div>'
        : '<div class="room-tag">Закрыто</div>';
      const lock = r.open ? '○' : '⚿';
      return `
        <div class="room-card animate-in" data-room="${esc(r.name)}" data-open="${r.open}"
             style="animation-delay:${i * 0.06}s">
          <div class="room-num">${String(i+1).padStart(2,'0')}/</div>
          <div class="room-lock">${lock}</div>
          <div class="room-name">${esc(r.name)}</div>
          <div class="room-desc">${esc(r.desc)}</div>
          <div class="room-footer">
            <div class="room-count">${r.online} / ${r.maxSlots} ${word}</div>
            ${tag}
          </div>
        </div>`;
    }).join('');

    // Клики через делегирование
    grid.onclick = e => {
      const card = e.target.closest('.room-card');
      if (card) openModal(card.dataset.room, card.dataset.open === 'true');
    };

  } catch {
    grid.innerHTML = `
      <div class="rooms-error">
        Сервер недоступен.<br>
        <small>Запусти server.py и обнови страницу.</small>
      </div>`;
    if (counter) counter.textContent = 'Сервер офлайн';
  }
}

// ══════════════════════════════════════════════
// MODAL
// ══════════════════════════════════════════════
let activeRoomName = null;
let activeToken    = null;
let pollTimer      = null;

function openModal(roomName, isOpen) {
  activeRoomName = roomName;
  activeToken    = null;

  document.getElementById('mEyebrow').textContent = 'Комната · ' + roomName;
  document.getElementById('mTitle').textContent   = roomName;
  document.getElementById('mErrMsg').style.display = 'none';
  document.getElementById('mMembers').innerHTML   = '';

  const passWrap = document.getElementById('mPassWrap');

  if (isOpen) {
    passWrap.style.display = 'none';
    verifyAndLoad(roomName, '');
  } else {
    passWrap.style.display = 'block';
    document.getElementById('mInput').value = '';
    document.getElementById('mInput').classList.remove('err');
    setTimeout(() => document.getElementById('mInput').focus(), 150);
  }

  document.getElementById('modalBg').classList.add('open');
}

function closeModal() {
  document.getElementById('modalBg').classList.remove('open');
  activeRoomName = null;
  activeToken    = null;
  clearInterval(pollTimer);
}

async function checkPass() {
  const inp = document.getElementById('mInput');
  const err = document.getElementById('mErrMsg');
  const password = inp.value;

  if (!password) { inp.classList.add('err'); return; }

  inp.disabled = true;
  document.getElementById('mSubmit').textContent = 'Проверка...';

  await verifyAndLoad(activeRoomName, password);

  inp.disabled = false;
  document.getElementById('mSubmit').textContent = 'Войти →';
}

async function verifyAndLoad(roomName, password) {
  const inp = document.getElementById('mInput');
  const err = document.getElementById('mErrMsg');

  try {
    const res = await fetch(`${SERVER_URL}/api/rooms/${encodeURIComponent(roomName)}/verify`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ password }),
    });

    if (res.status === 401) {
      inp.classList.add('err');
      err.style.display = 'block';
      inp.animate([
        { transform: 'translateX(-5px)' },
        { transform: 'translateX(5px)'  },
        { transform: 'translateX(-3px)' },
        { transform: 'translateX(0)'    }
      ], { duration: 280 });
      return;
    }

    if (!res.ok) throw new Error('Server error');

    const data = await res.json();
    activeToken = data.token;

    inp.classList.remove('err');
    err.style.display = 'none';
    document.getElementById('mPassWrap').style.display = 'none';

    // Загружаем участников и запускаем polling
    await loadMembers();
    clearInterval(pollTimer);
    pollTimer = setInterval(loadMembers, typeof POLL_INTERVAL !== 'undefined' ? POLL_INTERVAL : 5000);

  } catch {
    err.textContent   = 'Сервер недоступен. Запусти server.py';
    err.style.display = 'block';
  }
}

async function loadMembers() {
  if (!activeRoomName || !activeToken) return;

  try {
    const res = await fetch(
      `${SERVER_URL}/api/rooms/${encodeURIComponent(activeRoomName)}/members`,
      { headers: { 'X-Room-Token': activeToken } }
    );

    if (!res.ok) return;
    const data = await res.json();
    renderMembers(data.members || [], data.total || 0);

  } catch { /* тихо игнорируем ошибки polling */ }
}

function renderMembers(members, total) {
  const wrap = document.getElementById('mMembers');
  const word = memberWord(total);

  const rows = members.map(m => {
    const isDetected = m.status === 'detected';
    const statusClass = isDetected ? 'status-detected' : 'status-clean';
    const statusIcon  = isDetected ? '⚠ Обнаружен софт' : '✓ Чист';
    const connClass   = m.connected ? 'member-online' : 'member-offline';
    const connText    = m.connected ? 'Подключён' : 'Проблемы с подключением';

    return `
      <div class="member-item">
        <div class="member-left">
          <div class="member-ava">${esc(getInitials(m.nick))}</div>
          <div class="member-info">
            <div class="member-name">${esc(m.nick)}</div>
            <div class="member-sid">${esc(m.steamId)}</div>
          </div>
        </div>
        <div class="member-right">
          <div class="${statusClass}">${statusIcon}</div>
          <div class="member-conn">
            <span class="${connClass}"></span>
            ${esc(connText)}
          </div>
        </div>
      </div>`;
  }).join('');

  wrap.innerHTML = `
    <hr class="members-sep">
    <div class="members-header">${total} ${word} · онлайн</div>
    ${rows || '<div class="no-members">Участников пока нет · Включи античит</div>'}
  `;
}

// ══════════════════════════════════════════════
// MODAL СОБЫТИЯ
// ══════════════════════════════════════════════
document.getElementById('mInput').addEventListener('keydown', e => {
  if (e.key === 'Enter') checkPass();
  document.getElementById('mInput').classList.remove('err');
  document.getElementById('mErrMsg').style.display = 'none';
});
document.addEventListener('keydown', e => { if (e.key === 'Escape') closeModal(); });
document.getElementById('modalBg').addEventListener('click', e => {
  if (e.target === document.getElementById('modalBg')) closeModal();
});

// ══════════════════════════════════════════════
// INIT
// ══════════════════════════════════════════════
renderStats();
renderTrust();
