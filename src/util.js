// Shared utility functions

// -- API --

async function api(method, path, { body } = {}) {
	const opts = { method, headers: {}, credentials: "same-origin" };
	if (body !== undefined) {
		opts.headers["Content-Type"] = "application/json";
		opts.body = JSON.stringify(body);
	}
	const resp = await fetch(path, opts);
	let data = null;
	const text = await resp.text();
	try { data = JSON.parse(text); } catch { data = text; }
	return { status: resp.status, data };
}

// -- User state --

// Shared current user state: { user_id, username } or null
let currentUser = null;

// Fetch current user from server. Returns the user object or null.
async function loadCurrentUser() {
	const r = await api("GET", "/api/user");
	if (r.status === 200) {
		currentUser = { user_id: r.data.user_id, username: r.data.username, bot_assist: r.data.bot_assist, is_anon: r.data.is_anon };
	} else {
		currentUser = null;
	}
	return currentUser;
}

// Ensure we have a session (creating anon if needed). Return false if we failed
// to ensure the session exists.
async function ensureSession() {
	const r = await api("GET", "/api/user");
	if (r.status === 200) {
		currentUser = { user_id: r.data.user_id, username: r.data.username, bot_assist: r.data.bot_assist, is_anon: r.data.is_anon };
		return true;
	}
	const a = await api("POST", "/api/anon_user");
	if (a.status === 204) {
		await loadCurrentUser();
		return true;
	}
	return false;
}

async function doLogout() {
	await api("POST", "/api/user/logout");
	currentUser = null;
}

// -- Header --

// Render the header-right element with username/logout or login button.
// onLogout is an optional callback for page-specific cleanup after logout.
function renderHeader(onLogout) {
	const el = document.getElementById("header-right");
	if (!el) return;
	el.innerHTML = "";
	if (currentUser) {
		const span = document.createElement("span");
		span.className = "username";
		span.textContent = currentUser.username;
		span.style.cursor = "pointer";
		span.onclick = () => { window.location.href = "/user/" + currentUser.user_id; };
		el.appendChild(span);
		const btn = document.createElement("button");
		btn.className = "btn-secondary btn-small";
		btn.textContent = "Logout";
		btn.onclick = async () => {
			await doLogout();
			renderHeader(onLogout);
			if (onLogout) onLogout();
		};
		el.appendChild(btn);
	} else {
		const btn = document.createElement("button");
		btn.className = "btn-primary btn-small";
		btn.textContent = "Login";
		btn.onclick = () => { window.location.href = "/login"; };
		el.appendChild(btn);
	}
}

// -- Panning --

// Set up canvas panning. Returns an object with { getCam, setCam } for reading/writing camX/camY.
// renderFn is called on every pan movement.
// onClickFn (optional) is called when user clicks without dragging.
function setupPanning(canvas, renderFn, onClickFn) {
	let camX = 0, camY = 0;
	let dragging = false, dragStartX = 0, dragStartY = 0, camStartX = 0, camStartY = 0;

	canvas.addEventListener("mousedown", (e) => {
		dragging = true;
		dragStartX = e.clientX;
		dragStartY = e.clientY;
		camStartX = camX;
		camStartY = camY;
	});

	window.addEventListener("mousemove", (e) => {
		if (!dragging) return;
		camX = camStartX - (e.clientX - dragStartX);
		camY = camStartY - (e.clientY - dragStartY);
		renderFn();
	});

	window.addEventListener("mouseup", (e) => {
		if (!dragging) return;
		dragging = false;
		const dx = e.clientX - dragStartX;
		const dy = e.clientY - dragStartY;
		if (Math.abs(dx) < 5 && Math.abs(dy) < 5 && onClickFn) {
			onClickFn(e);
		}
	});

	const cam = {
		get x() { return camX; },
		get y() { return camY; },
		set x(v) { camX = v; },
		set y(v) { camY = v; },
		reset() { camX = 0; camY = 0; renderFn(); },
	};
	return cam;
}

// -- Game list entries --

// Create a game entry element for a game list.
// params:
//   game: Game object
//   opts: { selected: bool, onMouseEnter: fn }
function createGameEntry(game, opts = {}) {
	const entry = document.createElement("div");
	entry.className = "game-entry" + (opts.selected ? " selected" : "");
	entry.onclick = () => { window.location.href = "/game/" + game.game_id; };
	if (opts.onMouseEnter) entry.onmouseenter = opts.onMouseEnter;

	const p1 = document.createElement("span");
	p1.className = "p1";
	p1.textContent = formatUname(game.p1_uname, game.p1_uid, game.is_started);

	const mid = document.createElement("span");
	mid.className = "moves";
	mid.textContent = "Move " + game.total_moves;

	const p2 = document.createElement("span");
	p2.className = "p2";
	p2.textContent = formatUname(game.p2_uname, game.p2_uid, game.is_started);

	const winnerSide = game.winner_index;
	if (!game.is_complete) {
		// do not add any classes
	} else if (winnerSide === 1) {
		p1.classList.add("winner");
	} else if (winnerSide === 2) {
		p2.classList.add("winner");
	} else if (winnerSide === 0) {
		p1.classList.add("draw");
		p2.classList.add("draw");
	}

	entry.appendChild(p1);
	entry.appendChild(mid);
	entry.appendChild(p2);
	return entry;
}

// -- Win detection --

// moves_tbl: Map with key "a,r,c" -> move index
// pos: {a, r, c}
// Returns array of move indices forming the line, or [] if no line of 6.

// r and c must be between -16384 and 16384
function _getKey(a, r, c) {
	let key = a & 1;
	key = key << 15;
	key = key | (r & 0x7FFF);
	key = key << 15;
	key = key | (c & 0x7FFF);
	return key;
}

function _getARCFromKey(key) {
	// Bit magic to sign extend the first 15 bits
	let c = ((key & 0x7FFF) << (32 - 15)) >> (32 - 15);
	key = key >>> 15;
	let r = ((key & 0x7FFF) << (32 - 15)) >> (32 - 15);
	key = key >>> 15;
	let a = key & 1;
	return { a, r, c };
}

function _getPosKey(pos) { return _getKey(pos.a, pos.r, pos.c); }

function _lineIndices(movesTbl, pos, stepFn) {
	const key = _getPosKey(pos);
	const startEntry = movesTbl.get(key);
	if (startEntry === undefined) return [];
	const startIdx = startEntry.index;
	const isP1 = startEntry.isP1;

	const indices = [startIdx];
	let ca = pos.a, cr = pos.r, cc = pos.c;
	for (let i = 0; i < 5; i++) {
		const next = stepFn(ca, cr, cc);
		ca = next.a; cr = next.r; cc = next.c;
		const k = _getPosKey(next);
		const nextEntry = movesTbl.get(k);
		if (nextEntry === undefined || nextEntry.isP1 !== isP1) return [];
		const idx = nextEntry.index;
		indices.push(idx);
	}
	return indices;
}

function _stepR(a, r, c) { return { a, r, c: c + 1 }; }

function _stepL(a, r, c) { return { a, r, c: c - 1 }; }

function _stepDR(a, r, c) {
	const nr = r + (a & 1);
	const nc = c + (a & 1);
	return { a: 1 - a, r: nr, c: nc };
}

function _stepDL(a, r, c) {
	const na = 1 - a;
	const nr = r + (a & 1);
	const nc = c - (na & 1);
	return { a: na, r: nr, c: nc };
}

function _stepUR(a, r, c) {
	const na = 1 - a;
	const nr = r - (na & 1);
	const nc = c + (a & 1);
	return { a: na, r: nr, c: nc };
}

function _stepUL(a, r, c) {
	const na = 1 - a;
	const nr = r - (na & 1);
	const nc = c - (na & 1);
	return { a: na, r: nr, c: nc };
}

function movesToTbl(moves) {
	const movesTbl = new Map();
	for (let i = 0; i < moves.length; i++) {
		const m = moves[i];
		const k = _getKey(m.a, m.r, m.c);
		movesTbl.set(k, {index: i, isP1: m.p1});
	}
	return movesTbl;
}

// Find the indices of the 6 winning moves, or null if no winner.
// moves: array of {a, r, c, p1}
function findWinningMoves(moves) {
	const tbl = movesToTbl(moves);
	for (const m of moves) {
		const pos = { a: m.a, r: m.r, c: m.c };
		for (const stepFn of [_stepR, _stepDR, _stepDL]) {
			const indices = _lineIndices(tbl, pos, stepFn);
			if (indices.length === 6) return indices;
		}
	}
	return null;
}

// -- Helpers --

function formatUname(uname, uid, is_started) {
	if (uid && is_started) {
		return uname || "[Deleted]";
	} else {
		return uname || "...";
	}
}
