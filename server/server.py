"""
ASTI ANTI-CHEAT — Python сервер
Запуск: python server.py
"""

import os
import json
import time
import hashlib
import secrets
from flask import Flask, request, jsonify, abort
from flask_cors import CORS

app = Flask(__name__)
CORS(app, origins=["*"])  # В продакшне укажи свой домен

# ══════════════════════════════════════════════
# КОНФИГУРАЦИЯ
# ══════════════════════════════════════════════
CONFIG = {
    "version":          "1.0.0",
    "heartbeat_timeout": 90,      # секунд до офлайна
    "max_room_members":  50,
    "api_key":          os.environ.get("ASTI_API_KEY", "change_me_in_production"),
}

# ══════════════════════════════════════════════
# КОМНАТЫ (пароли хранятся как SHA-256 хэши)
# Чтобы добавить комнату — добавь элемент в ROOMS
# Пароль хэшируй: python -c "import hashlib; print(hashlib.sha256(b'твойпароль').hexdigest())"
# ══════════════════════════════════════════════
def hash_password(plain: str) -> str:
    return hashlib.sha256(plain.encode()).hexdigest()

ROOMS = {
    "OPEN": {
        "name":          "OPEN",
        "desc":          "Открытая комната · Без пароля",
        "open":          True,
        "password_hash": None,
        "max_slots":     10,
    },
    "VOEX": {
        "name":          "VOEX",
        "desc":          "for Voex NEws",
        "open":          False,
        "password_hash": "2618be5da8aefa55ea5834d506110cf6fab41a09236ffaa6798f8a1a83125a9c",
        "max_slots":     20,
    },
    "-": {
        "name":          "-",
        "desc":          "-",
        "open":          False,
        "password_hash": "8ee7121d5e401d852cd3342af65511daa9e49932c6f011d79c1b81408e976442",
        "max_slots":     20,
    },
    "-": {
        "name":          "-",
        "desc":          "-",
        "open":          False,
        "password_hash": "22ad18a03fd26627225366c2337f1c93693c89fc89b62b8dff3d393e9761d139",
        "max_slots":     20,
    },
    "-": {
        "name":          "-",
        "desc":          "-",
        "open":          False,
        "password_hash": "0d3d6a039df440e724228eba80ac3fdb072ca90655554ddabb01b344da8bfe3d",
        "max_slots":     10,
    },
    "-": {
        "name":          "-",
        "desc":          "-",
        "open":          False,
        "password_hash": "865a534662a8882c1475bcc27a71646777fef9ed82c8cf9e0462349f186ca7d1",
        "max_slots":     10,
    },
}

# ══════════════════════════════════════════════
# ХРАНИЛИЩЕ УЧАСТНИКОВ (в памяти)
# В продакшне замени на SQLite или Redis
# ══════════════════════════════════════════════
# Структура: { "ROOM_NAME": { "hwid": { ...данные... } } }
members: dict = {name: {} for name in ROOMS}

def get_online_members(room_name: str) -> list:
    """Возвращает только онлайн-участников комнаты."""
    now = time.time()
    room = members.get(room_name, {})
    online = []
    for hwid, m in room.items():
        if now - m.get("last_seen", 0) <= CONFIG["heartbeat_timeout"]:
            online.append(m)
    return sorted(online, key=lambda x: x.get("joined_at", 0))

def clean_offline_members():
    """Удаляет давно офлайн участников (> 5 минут)."""
    now = time.time()
    for room_name in members:
        offline = [
            hwid for hwid, m in members[room_name].items()
            if now - m.get("last_seen", 0) > 300
        ]
        for hwid in offline:
            del members[room_name][hwid]

# ══════════════════════════════════════════════
# ХЕЛПЕРЫ
# ══════════════════════════════════════════════
def validate_api_key(req) -> bool:
    key = req.headers.get("X-API-Key", "")
    return secrets.compare_digest(key, CONFIG["api_key"])

def sanitize_string(s, max_len=100) -> str:
    if not isinstance(s, str):
        return "Unknown"
    # Убираем опасные символы
    s = s.strip()[:max_len]
    return s if s else "Unknown"

def validate_steam_id(sid: str) -> bool:
    """Steam ID — 17 цифр начинающихся с 7656."""
    return (
        isinstance(sid, str) and
        len(sid) == 17 and
        sid.isdigit() and
        sid.startswith("7656")
    )

# ══════════════════════════════════════════════
# РОУТЫ — КЛИЕНТ (античит .exe)
# ══════════════════════════════════════════════

@app.route("/api/launch", methods=["POST"])
def api_launch():
    """Вызывается когда игрок запускает античит."""
    if not validate_api_key(request):
        abort(403)

    data = request.get_json(silent=True) or {}

    steam_id  = sanitize_string(data.get("steamId",  ""), 17)
    nick      = sanitize_string(data.get("nick",      "Unknown"), 64)
    mac       = sanitize_string(data.get("mac",       ""), 17)
    hwid      = sanitize_string(data.get("hwid",      ""), 64)
    room_name = sanitize_string(data.get("room",      "OPEN"), 32).upper()
    ac_ver    = sanitize_string(data.get("acVersion", "1.0"), 10)

    if not validate_steam_id(steam_id):
        return jsonify({"ok": False, "error": "Invalid Steam ID"}), 400

    if room_name not in ROOMS:
        room_name = "OPEN"

    now = time.time()
    members[room_name][hwid] = {
        "hwid":       hwid,
        "steamId":    steam_id,
        "nick":       nick,
        "mac":        mac,
        "room":       room_name,
        "acVersion":  ac_ver,
        "status":     "clean",       # clean | detected
        "connected":  True,
        "joined_at":  now,
        "last_seen":  now,
    }

    clean_offline_members()
    print(f"[+] Launch: {nick} ({steam_id}) → комната {room_name}")
    return jsonify({"ok": True, "room": room_name})


@app.route("/api/heartbeat", methods=["POST"])
def api_heartbeat():
    """Пинг каждые N секунд — подтверждает что игрок онлайн."""
    if not validate_api_key(request):
        abort(403)

    data      = request.get_json(silent=True) or {}
    hwid      = sanitize_string(data.get("hwid", ""), 64)
    room_name = sanitize_string(data.get("room", "OPEN"), 32).upper()

    if room_name not in ROOMS:
        room_name = "OPEN"

    if hwid in members.get(room_name, {}):
        members[room_name][hwid]["last_seen"]  = time.time()
        members[room_name][hwid]["connected"]  = True

    return jsonify({"ok": True})


@app.route("/api/scan", methods=["POST"])
def api_scan():
    """Результат сканирования от клиента."""
    if not validate_api_key(request):
        abort(403)

    data      = request.get_json(silent=True) or {}
    hwid      = sanitize_string(data.get("hwid", ""), 64)
    room_name = sanitize_string(data.get("room", "OPEN"), 32).upper()
    cheats    = bool(data.get("cheatsFound", False))
    processes = data.get("processes", [])

    if not isinstance(processes, list):
        processes = []
    processes = [sanitize_string(p, 64) for p in processes[:20]]

    if room_name in ROOMS and hwid in members.get(room_name, {}):
        members[room_name][hwid]["status"]    = "detected" if cheats else "clean"
        members[room_name][hwid]["last_seen"] = time.time()
        members[room_name][hwid]["cheats"]    = processes

        if cheats:
            nick = members[room_name][hwid].get("nick", "?")
            print(f"[!] ЧИТ: {nick} — {processes}")

    return jsonify({"ok": True})


@app.route("/api/disconnect", methods=["POST"])
def api_disconnect():
    """Вызывается когда игрок закрывает античит."""
    if not validate_api_key(request):
        abort(403)

    data      = request.get_json(silent=True) or {}
    hwid      = sanitize_string(data.get("hwid", ""), 64)
    room_name = sanitize_string(data.get("room", "OPEN"), 32).upper()

    if room_name in ROOMS and hwid in members.get(room_name, {}):
        del members[room_name][hwid]

    return jsonify({"ok": True})


# ══════════════════════════════════════════════
# РОУТЫ — САЙТ
# ══════════════════════════════════════════════

@app.route("/api/rooms", methods=["GET"])
def api_rooms():
    """Список всех комнат (без паролей!)."""
    result = []
    for name, room in ROOMS.items():
        online = get_online_members(name)
        result.append({
            "name":     room["name"],
            "desc":     room["desc"],
            "open":     room["open"],
            "maxSlots": room["max_slots"],
            "online":   len(online),
        })
    return jsonify(result)


@app.route("/api/rooms/<room_name>/verify", methods=["POST"])
def api_room_verify(room_name):
    """Проверка пароля комнаты. Возвращает токен сессии."""
    room_name = room_name.upper()
    if room_name not in ROOMS:
        abort(404)

    room = ROOMS[room_name]
    if room["open"]:
        # Открытая комната — пароль не нужен
        token = secrets.token_hex(16)
        return jsonify({"ok": True, "token": token})

    data     = request.get_json(silent=True) or {}
    password = data.get("password", "")

    if not isinstance(password, str) or len(password) > 128:
        return jsonify({"ok": False, "error": "Invalid"}), 400

    hashed = hash_password(password)
    if secrets.compare_digest(hashed, room["password_hash"]):
        token = secrets.token_hex(16)
        return jsonify({"ok": True, "token": token})
    else:
        # Задержка против брутфорса
        time.sleep(0.5)
        return jsonify({"ok": False, "error": "Wrong password"}), 401


@app.route("/api/rooms/<room_name>/members", methods=["GET"])
def api_room_members(room_name):
    """Список участников комнаты."""
    room_name = room_name.upper()
    if room_name not in ROOMS:
        abort(404)

    # Простая проверка токена через заголовок
    # В продакшне используй JWT
    token = request.headers.get("X-Room-Token", "")
    if not token or len(token) != 32:
        abort(401)

    online = get_online_members(room_name)

    # Не отдаём HWID и MAC на сайт — только нужные поля
    safe = [{
        "nick":      m["nick"],
        "steamId":   m["steamId"],
        "status":    m["status"],
        "connected": True,
        "joinedAt":  int(m["joined_at"]),
    } for m in online]

    return jsonify({
        "room":    room_name,
        "members": safe,
        "total":   len(safe),
    })


@app.route("/api/health", methods=["GET"])
def health():
    return jsonify({"ok": True, "version": CONFIG["version"]})


# ══════════════════════════════════════════════
# ЗАПУСК
# ══════════════════════════════════════════════
if __name__ == "__main__":
    print("=" * 50)
    print("  ASTI ANTI-CHEAT SERVER v1.0")
    print("=" * 50)
    print(f"  API Key: {CONFIG['api_key']}")
    print(f"  Комнат: {len(ROOMS)}")
    print("  Запуск на http://localhost:5000")
    print("=" * 50)
    import os
    port = int(os.environ.get("PORT", 5000))
    app.run(host="0.0.0.0", port=port, debug=False)
