const P1_WON = 262144;
const P2_WON = -262144;
const STEP_FN_PAIRS = [[_stepR, _stepL], [_stepDR, _stepUL], [_stepDL, _stepUR]];
const MAX_EVAL_DEPTH = 5;

// At earlier depths, consider many moves. At later depths, consider fewer.
function lookMovesAtDepth(depth) {
	if (depth < 4) {
		return 11 - depth*2;
	}
	return 3;
}

// Do not rely on the move index in the movesTbl value; it may not be set.
function scoreLine(movesTbl, pos, stepFn, negStepFn) {
	const key = _getPosKey(pos);
	const startEntry = movesTbl.get(key);
	if (startEntry === undefined) return {score: 0, moves: []};
	const isP1 = startEntry.isP1;

	let ca = pos.a, cr = pos.r, cc = pos.c;
	const prev = negStepFn(ca, cr, cc);
	const prevKey = _getPosKey(prev);
	const prevEntry = movesTbl.get(prevKey);
	// If the previous entry is the same as the current one, we just return 0.
	// We only evaluate the end of a line, because we don't want to double count
	// scores.
	if (prevEntry !== undefined && prevEntry.isP1 == isP1) {
		return {score: 0, moves: []};
	}
	// If this line is terminated on the prev side, we will single count the score.
	// If it's unterminated on the prev side, we'll double count it, because we have two
	// opportunities to use this line.
	const terminated = prevEntry !== undefined;
	// If we are p1, the score must be positive. If we are p1, the score must be
	// negative.
	const negateScore = isP1 ? 1 : -1;
	for (let i = 0; i < 5; i++) {
		let score = i+1;
		const next = stepFn(ca, cr, cc);
		const nextKey = _getPosKey(next);
		const nextEntry = movesTbl.get(nextKey);
		// If we've reached the end of the line, and there's nothing blocking it,
		// then we can return the score (doubled if not terminated on the prev side)
		if (nextEntry === undefined) {
			// We want to make high scores much more desirable. Otherwise, points of interest,
			// a.k.a. locations with many nearby tiles, get overrepresented.
			score = score * score * score;
			if (terminated) {
				// A good candidate move is at the end of this line
				return {score: score * negateScore,
						moves: [nextKey]}
			} else {
				// At the end of both lines is good if possible
				return {score: score*2 * negateScore,
						moves: [prevKey, nextKey]}
			}
		}
		// If we find an opponent tile, we've ended here. We may have 0 score,
		// or normal score if the other side is not terminated.
		if (nextEntry.isP1 != isP1) {
			score = score * score * score;
			if (terminated) {
				return {score: 0, moves: []};
			} else {
				return {score: score * negateScore,
						moves: [prevKey]};
			}
		}
		ca = next.a; cr = next.r; cc = next.c;
	}
	
	// We never hit an opponent or a blank spot. So that means we have 6 in a row,
	// which is winning outright. Return P1_WON or P2_WON to represent that.
	return isP1 ? {score: P1_WON} : {score: P2_WON};
}

// Iterate over every move in the current board state. For each move, check all the
// directions to see if it's part of a line (we check right, down right, down left,
// since we don't need to check the opposite directions).
// Each move will give us a score and some candidate moves, or undefined.
// If we get undefined, it means that move is connected to a winning row.
// If we get score = 0, then it's a no-op.
// Otherwise, we put the move into the candidates table, adding the score if the
// move is already in the table. We also add up the total score for every move.
// Do not rely on the move index in the movesTbl value; it may not be set.
function evaluateBoard(movesTbl) {
	const candidateMovesTbl = new Map();
	let totalScore = 0;
	for (const mk of movesTbl.keys()) {
		const pos = _getARCFromKey(mk);
		for (const stepFns of STEP_FN_PAIRS) {
			let subscore = scoreLine(movesTbl, pos, stepFns[0], stepFns[1]);
			// If we got a winner, just return that.
			if (subscore.score == P1_WON || subscore.score == P2_WON) {
				return {totalScore: subscore.score};
			}
			// If this move got scored 0, we don't care about it at all.
			if (subscore.score == 0) {
				continue;
			}
			totalScore += subscore.score;
			// Go over every candidate move that was returned, and put them into the candidate table.
			// Either add the score to existing, or set it if it doesn't already exist.
			// This should only be 1 or 2 moves, since a line only has two sides...
			for (const cm of subscore.moves) {
				let cmScore = candidateMovesTbl.get(cm);
				if (cmScore === undefined) {
					cmScore = 0;
				}
				// All we care about is how impactful this move is. That is a sum
				// of its value to both players.
				if (subscore.score > 0) {
					cmScore += subscore.score;
				} else {
					cmScore -= subscore.score;
				}
				candidateMovesTbl.set(cm, cmScore);
			}
		}
	}

	// At this point, we've measured the total score and collated scores for each candidate move.
	return {totalScore: totalScore, candidateMoves: candidateMovesTbl};
}

function isP1ForTurn(numMoves) {
	if (numMoves == 0) {
		return true;
	}
	if (Math.floor((numMoves - 1) / 2) % 2 == 0) {
		return false;
	}
	return true;
}

// This function chooses the best move for the current player, based on the moves
// passed in. Do not rely on the move index in the movesTbl value; it may not be set.
// movesTbl is shared between calls to this function, it must
// be returned to its original state when this function exits.
function evaluateAhead(movesTbl, depth) {
	const isP1 = isP1ForTurn(movesTbl.size);
	const evaluation = evaluateBoard(movesTbl);
	const currentEval = evaluation.totalScore;
	// If one of the players won, just return that info. There's nothing else to
	// do here.
	if(currentEval == P1_WON || currentEval == P2_WON) {
		// If we need to return the best move, but the game is over, just
		// give undefined. There is no move to do.
		if (depth === 0) {
			return undefined;
		} else {
			return currentEval;
		}
	}
	// If this is our max depth, just return the current eval without finding anything more accurate.
	if (depth >= MAX_EVAL_DEPTH) {
		return currentEval;
	}
	// Now we need to go through the moves and evaluate them in order. The moves
	// are labeled with a score of how impactful they are. We simply go through them
	// in order of impact, and see which gives us the best evaluation.
	const candidatesTbl = evaluation.candidateMoves;
	// Sort in order of impact, descending.
	const sortedCandidates = [...candidatesTbl.entries()].sort(([iA, scA], [iB, scB]) => {
		return scB - scA;
	});
	let lookMoves = lookMovesAtDepth(depth);
	if (lookMoves > sortedCandidates.length) {
		lookMoves = sortedCandidates.length;
	}
	let bestEval = undefined;
	let bestMoveKey = undefined;
	for (let i = 0; i < lookMoves; i++) {
		const candidate = sortedCandidates[i];
		const moveKey = candidate[0];
		// console.log(`Evaluating move ${_getARCFromKey(moveKey)} for depth ${depth} as P1? ${isP1}...`);
		movesTbl.set(moveKey, {index: -1, isP1: isP1});
		const moveEval = evaluateAhead(movesTbl, depth+1);
		movesTbl.delete(moveKey);
		// console.log(`Move ${_getARCFromKey(moveKey)} for P1? ${isP1} for depth ${depth} has score ${moveEval}`);
		// If this is the first try, or this move is better for us than the previous
		// best, update the best move.
		if (bestEval === undefined ||
			(isP1 && moveEval > bestEval) ||
			(!isP1 && moveEval < bestEval)
		) {
			bestEval = moveEval;
			bestMoveKey = moveKey;
		}
		// If the current player wins with this move, stop looking for other moves.
		if ((isP1 && bestEval == P1_WON) ||
			(!isP1 && bestEval == P2_WON)) {
				break;
		}
	}
	// If we're at the first step, the caller wants the best move.
	// If we're not at the first step, the caller wants the best evaluation.
	if (depth === 0) {
		return bestMoveKey;
	} else {
		return bestEval;
	}
}