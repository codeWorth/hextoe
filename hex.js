// Shared hex grid rendering logic for HexToe
// Used by game.html and home.html

const HEX_SIZE = 28;
const HEX_W = Math.sqrt(3) * HEX_SIZE;
const HEX_H = 2 * HEX_SIZE;

function hexToPixel(a, r, c) {
	const row = r * 2 + a;
	const px = c * HEX_W + (row % 2 !== 0 ? HEX_W / 2 : 0);
	const py = row * HEX_H * 0.75;
	return { x: px, y: py };
}

function pixelToHex(px, py) {
	const rowApprox = py / (HEX_H * 0.75);
	const rowRound = Math.round(rowApprox);

	let bestDist = Infinity, bestA = 0, bestR = 0, bestC = 0;
	for (let rowOff = -1; rowOff <= 1; rowOff++) {
		const row = rowRound + rowOff;
		const a = ((row % 2) + 2) % 2;
		const r = (row - a) / 2;
		const xOff = (row % 2 === 1) ? HEX_W / 2 : 0;
		const cApprox = (px - xOff) / HEX_W;
		for (let cOff = -1; cOff <= 1; cOff++) {
			const c = Math.round(cApprox) + cOff;
			const center = hexToPixel(a, r, c);
			const dx = px - center.x;
			const dy = py - center.y;
			const dist = dx * dx + dy * dy;
			if (dist < bestDist) {
				bestDist = dist;
				bestA = a; bestR = r; bestC = c;
			}
		}
	}
	return { a: bestA, r: bestR, c: bestC };
}

function drawHex(ctx, cx, cy, size, fill, stroke) {
	ctx.beginPath();
	for (let i = 0; i < 6; i++) {
		const angle = Math.PI / 180 * (60 * i - 30);
		const x = cx + size * Math.cos(angle);
		const y = cy + size * Math.sin(angle);
		if (i === 0) ctx.moveTo(x, y);
		else ctx.lineTo(x, y);
	}
	ctx.closePath();
	if (fill) { ctx.fillStyle = fill; ctx.fill(); }
	if (stroke) { ctx.strokeStyle = stroke; ctx.lineWidth = 1.5; ctx.stroke(); }
}

function drawX(ctx, cx, cy, size) {
	const s = size * 0.45;
	ctx.strokeStyle = "#ddd";
	ctx.lineWidth = 3;
	ctx.lineCap = "round";
	ctx.beginPath();
	ctx.moveTo(cx - s, cy - s); ctx.lineTo(cx + s, cy + s);
	ctx.moveTo(cx + s, cy - s); ctx.lineTo(cx - s, cy + s);
	ctx.stroke();
}

function drawO(ctx, cx, cy, size) {
	const r = size * 0.4;
	ctx.strokeStyle = "#ddd";
	ctx.lineWidth = 3;
	ctx.lineCap = "round";
	ctx.beginPath();
	ctx.arc(cx, cy, r, 0, Math.PI * 2);
	ctx.stroke();
}

// Renders the hex grid and moves onto the given canvas.
// camX/camY are camera offsets in pixel space.
// moves is an array of {a, r, c, p1} objects (can be null/empty).
function renderHexGrid(canvas, ctx, camX, camY, moves, highlightIndex, winMoves) {
	const dpr = window.devicePixelRatio || 1;
	canvas.style.width = "0";
	canvas.style.height = "0";
	const rect = canvas.parentElement.getBoundingClientRect();
	const w = rect.width;
	const h = rect.height;
	canvas.style.width = w + "px";
	canvas.style.height = h + "px";
	canvas.width = w * dpr;
	canvas.height = h * dpr;
	ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

	ctx.clearRect(0, 0, w, h);

	const centerX = w / 2 + camX;
	const centerY = h / 2 + camY;

	const visRows = Math.ceil(h / (HEX_H * 0.75)) + 4;
	const visCols = Math.ceil(w / HEX_W) + 4;

	const centerRow = Math.round((centerY - h / 2) / (HEX_H * 0.75));
	const centerCol = Math.round((centerX - w / 2) / HEX_W);

	for (let dr = -visRows; dr <= visRows; dr++) {
		const row = centerRow + dr;
		const a = ((row % 2) + 2) % 2;
		const r = (row - a) / 2;
		for (let dc = -visCols; dc <= visCols; dc++) {
			const c = centerCol + dc;
			const pos = hexToPixel(a, r, c);
			const sx = pos.x - camX + w / 2;
			const sy = pos.y - camY + h / 2;
			if (sx < -HEX_W || sx > w + HEX_W || sy < -HEX_H || sy > h + HEX_H) continue;
			drawHex(ctx, sx, sy, HEX_SIZE, "#2a2a2a", "#3a3a3a");
		}
	}

	if (!moves) return;

	for (let i = 0; i < moves.length; i++) {
		const move = moves[i];
		const pos = hexToPixel(move.a, move.r, move.c);
		const sx = pos.x - camX + w / 2;
		const sy = pos.y - camY + h / 2;
		if (sx < -HEX_W || sx > w + HEX_W || sy < -HEX_H || sy > h + HEX_H) continue;

		let isHighlight = false;
		if (highlightIndex >= 0 && i === highlightIndex) {
			isHighlight = true;
		} else if (highlightIndex < 0 && winMoves && winMoves.includes(i)) {
			isHighlight = true;
		}
		const fill = move.p1 ? "#3a2a2a" : "#2a2a3a";
		const stroke = isHighlight ? "#ddd" : (move.p1 ? "#6a4a4a" : "#4a4a6a");
		drawHex(ctx, sx, sy, HEX_SIZE, fill, stroke);
		if (move.p1) drawX(ctx, sx, sy, HEX_SIZE);
		else drawO(ctx, sx, sy, HEX_SIZE);
	}
}
