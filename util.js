// Utility helper functions

function formatUname(uname, uid) {
	if(uid) {
		return uname || "[Deleted]";
	} else {
		return uname || "...";
	}
}
