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
		currentUser = { user_id: r.data.user_id, username: r.data.username };
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
		currentUser = { user_id: r.data.user_id, username: r.data.username };
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

// -- Helpers --

function formatUname(uname, uid) {
	if (uid) {
		return uname || "[Deleted]";
	} else {
		return uname || "...";
	}
}
