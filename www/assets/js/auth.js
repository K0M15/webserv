/*
	Drives the Login/Logout link and the auth modal against the webserv
	backend (POST /login, POST /logout). The session cookie the server sets
	is HttpOnly, so the client can't read it directly - we mirror the last
	known result of /login and /logout in sessionStorage purely to decide
	what to render. sessionStorage (not localStorage) so a genuinely fresh
	page load doesn't keep claiming you're logged in indefinitely - it only
	lasts as long as the session cookie's own tab/browser session does. The
	server is always the source of truth for whether a request is actually
	authorized; gallery.js listens for the "webserv:authchange" event
	dispatched below to gate the upload form and the per-item delete
	buttons.
*/
(function() {

	var STORAGE_KEY = "webserv_logged_in";

	function isLoggedIn() {
		try {
			return sessionStorage.getItem(STORAGE_KEY) === "1";
		} catch (e) {
			return false;
		}
	}

	function setLoggedIn(value) {
		try {
			if (value)
				sessionStorage.setItem(STORAGE_KEY, "1");
			else
				sessionStorage.removeItem(STORAGE_KEY);
		} catch (e) {
			// sessionStorage unavailable (private browsing, etc) - nothing to do
		}
	}

	function showAuthMessage(text, isError) {
		var el = document.getElementById("authMessage");
		if (!el)
			return;
		el.textContent = text;
		el.classList.remove("error", "success");
		el.classList.add(isError ? "error" : "success");
	}

	function closeModal() {
		var overlay = document.getElementById("authModalOverlay");
		if (overlay)
			overlay.classList.remove("is-visible");
	}

	function openModal() {
		var overlay = document.getElementById("authModalOverlay");
		if (overlay)
			overlay.classList.add("is-visible");
	}

	function renderAuthState() {
		var link = document.getElementById("authLink");
		var loggedIn = isLoggedIn();

		if (link) {
			link.querySelector("span").textContent = loggedIn ? "Logout" : "Login";
			link.classList.toggle("fa-sign-out-alt", loggedIn);
			link.classList.toggle("fa-sign-in-alt", !loggedIn);
		}

		if (loggedIn)
			closeModal();

		document.dispatchEvent(new CustomEvent("webserv:authchange", { detail: { loggedIn: loggedIn } }));
	}

	function handleLogin(event) {
		event.preventDefault();

		var username = document.getElementById("authUsername").value;
		var password = document.getElementById("authPassword").value;
		var body = "username=" + encodeURIComponent(username) +
		           "&password=" + encodeURIComponent(password);

		fetch("/login", {
			method: "POST",
			headers: { "Content-Type": "application/x-www-form-urlencoded" },
			body: body
		}).then(function(response) {
			if (response.ok) {
				setLoggedIn(true);
				renderAuthState();
				showAuthMessage("Logged in.", false);
				document.getElementById("loginForm").reset();
			} else {
				showAuthMessage("Login failed (" + response.status + ").", true);
			}
		}).catch(function() {
			showAuthMessage("Could not reach the server.", true);
		});
	}

	function handleLogout() {
		fetch("/logout", { method: "POST" }).then(function(response) {
			setLoggedIn(false);
			renderAuthState();
			showAuthMessage(response.ok ? "Logged out." : "Logout returned " + response.status + ".", !response.ok);
		}).catch(function() {
			setLoggedIn(false);
			renderAuthState();
			showAuthMessage("Could not reach the server.", true);
		});
	}

	function onAuthLinkClick(event) {
		event.preventDefault();
		if (isLoggedIn())
			handleLogout();
		else
			openModal();
	}

	window.webservIsLoggedIn = isLoggedIn;

	document.addEventListener("DOMContentLoaded", function() {
		renderAuthState();

		var link = document.getElementById("authLink");
		if (link)
			link.addEventListener("click", onAuthLinkClick);

		var loginForm = document.getElementById("loginForm");
		if (loginForm)
			loginForm.addEventListener("submit", handleLogin);

		var closeButton = document.getElementById("authModalClose");
		if (closeButton)
			closeButton.addEventListener("click", closeModal);

		var overlay = document.getElementById("authModalOverlay");
		if (overlay) {
			overlay.addEventListener("click", function(event) {
				if (event.target === overlay)
					closeModal();
			});
		}

		document.addEventListener("keydown", function(event) {
			if (event.key === "Escape")
				closeModal();
		});
	});

})();
