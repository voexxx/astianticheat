# Asti Anti-Cheat

Система защиты от читов для CS2 с панелью мониторинга в реальном времени.

## Структура проекта

```
asti-anticheat/
├── server/           ← Python сервер
│   ├── server.py
│   └── requirements.txt
├── site/             ← Сайт-панель
│   ├── index.html
│   ├── style.css
│   ├── script.js
│   └── data.js
└── anticheat/        ← C++ клиент (Visual Studio)
    ├── Anticheat.cpp
    ├── fingerprint.h / .cpp
    ├── scanner.h / .cpp
    ├── reporter.h / .cpp
    └── config.h
```

## Быстрый старт

### 1. Сервер
```bash
cd server
pip install -r requirements.txt
python server.py
```

### 2. Сайт
Открой `site/index.html` в браузере.  
Или задеплой на хостинг (Netlify, GitHub Pages).

### 3. Клиент (C++)
Открой в Visual Studio, собери и запусти `Anticheat.exe`.

## Настройка

**Сменить API ключ:**
```bash
set ASTI_API_KEY=мой_секретный_ключ
python server.py
```

**Добавить комнату** — в `server/server.py` в словарь `ROOMS`:
```python
"MYROOM": {
    "name": "MYROOM",
    "desc": "Моя комната",
    "open": False,
    "password_hash": hash_password("мойпароль"),
    "max_slots": 20,
},
```

**Добавить организацию** — в `site/data.js`:
```js
{ name: 'МОЯ ОРГ' },
```

## Технологии

- **Клиент:** C++ / WinAPI
- **Сервер:** Python / Flask
- **Сайт:** HTML / CSS / JavaScript
