/*
	Renders the gallery grid: whatever's actually sitting in /uploads plus an
	always-trailing upload tile, paginated nine tiles per page. Everything
	here talks to the real webserv endpoints - POST /uploads/<file> to write
	an image (and a small companion static page carrying its title/
	description, since the server has no way to store metadata alongside a
	file), DELETE to remove one. DELETE is already gated server-side to the
	"admin" session role; POST is not (see auth.js's header comment) - the
	delete button is only hidden while logged out as a UI convenience, not a
	security boundary.

	There's no dedicated JSON listing API, but /uploads has dirindex on
	(see test.conf), so on load we fetch that HTML index, pair up each
	companion page with its image by shared basename, and pull the title
	out of the page's <h2>. That means the grid always reflects whatever's
	really on disk - no hardcoded seed list to keep in sync by hand. Once
	loaded, uploads/deletes during the session just mutate the in-memory
	list directly instead of re-hitting the listing endpoint.
*/
(function() {

	var PAGE_SIZE = 9;
	var UPLOADS_DIR = "/uploads/";

	var items = [];
	var currentPage = 1;

	function isLoggedIn() {
		return typeof window.webservIsLoggedIn === "function" && window.webservIsLoggedIn();
	}

	function setMessage(el, text, isError) {
		if (!el)
			return;
		el.textContent = text;
		el.classList.remove("error", "success");
		el.classList.add(isError ? "error" : "success");
	}

	function escapeHtml(str) {
		var div = document.createElement("div");
		div.textContent = str;
		return div.innerHTML;
	}

	function buildCompanionPage(title, description, imagePath) {
		var safeTitle = escapeHtml(title);
		var safeDescription = escapeHtml(description);
		var safeImage = escapeHtml(imagePath);
		return "<!DOCTYPE HTML>\n<html>\n<head>\n" +
			"<title>" + safeTitle + " - webserv</title>\n" +
			"<meta charset=\"utf-8\" />\n" +
			"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\" />\n" +
			"<link rel=\"stylesheet\" href=\"/assets/css/main.css\" />\n" +
			"<link rel=\"stylesheet\" href=\"/assets/css/custom.css\" />\n" +
			"</head>\n<body>\n<div class=\"gallery-page\">\n" +
			"<img src=\"" + safeImage + "\" alt=\"\" />\n" +
			"<h2>" + safeTitle + "</h2>\n" +
			"<p>" + safeDescription + "</p>\n" +
			"<a class=\"back-link icon solid fa-arrow-left\" href=\"/index.html\"><span>Back to gallery</span></a>\n" +
			"</div>\n</body>\n</html>\n";
	}

	function basename(name) {
		var dot = name.lastIndexOf(".");
		return dot > -1 ? name.substring(0, dot) : name;
	}

	function titleFromFilename(name) {
		var base = basename(name).replace(/^\d+-/, "");
		return base.replace(/[_-]+/g, " ").trim() || base;
	}

	function parseDirIndex(html) {
		var doc = new DOMParser().parseFromString(html, "text/html");
		var names = [];
		doc.querySelectorAll("a").forEach(function(a) {
			var name = a.textContent.trim();
			if (name)
				names.push(name);
		});
		return names;
	}

	function fetchTitle(pagePath, fallback) {
		return fetch(pagePath).then(function(response) {
			if (!response.ok)
				throw new Error("page fetch failed");
			return response.text();
		}).then(function(html) {
			var doc = new DOMParser().parseFromString(html, "text/html");
			var h2 = doc.querySelector(".gallery-page h2");
			var title = h2 && h2.textContent.trim();
			return title || fallback;
		}).catch(function() {
			return fallback;
		});
	}

	function loadItems() {
		return fetch(UPLOADS_DIR).then(function(response) {
			if (!response.ok)
				throw new Error("listing failed (" + response.status + ")");
			return response.text();
		}).then(function(html) {
			var names = parseDirIndex(html);
			var pageNames = names.filter(function(name) { return /\.html?$/i.test(name); });
			var imageNames = names.filter(function(name) { return !/\.html?$/i.test(name); });

			var pairs = pageNames.map(function(pageName) {
				var base = basename(pageName);
				var imageName = imageNames.filter(function(name) {
					return basename(name) === base;
				})[0];
				if (!imageName)
					return null;
				return {
					image: UPLOADS_DIR + imageName,
					page: UPLOADS_DIR + pageName,
					deletable: [ UPLOADS_DIR + imageName, UPLOADS_DIR + pageName ],
					fallbackTitle: titleFromFilename(imageName)
				};
			}).filter(function(item) { return item !== null; });

			return Promise.all(pairs.map(function(item) {
				return fetchTitle(item.page, item.fallbackTitle).then(function(title) {
					item.title = title;
					delete item.fallbackTitle;
					return item;
				});
			}));
		}).then(function(loadedItems) {
			loadedItems.sort(function(a, b) {
				return a.page < b.page ? 1 : (a.page > b.page ? -1 : 0);
			});
			items = loadedItems;
		});
	}

	function combinedTiles() {
		var tiles = items.map(function(item) {
			return { type: "item", data: item };
		});
		tiles.push({ type: "upload" });
		return tiles;
	}

	function totalPages() {
		return Math.max(1, Math.ceil(combinedTiles().length / PAGE_SIZE));
	}

	function removeItemFromState(item) {
		var index = items.indexOf(item);
		if (index > -1)
			items.splice(index, 1);
	}

	function deleteItem(item) {
		var paths = item.deletable;
		var pending = paths.length;
		var failed = false;

		function done() {
			pending--;
			if (pending > 0)
				return;
			var galleryMsg = document.getElementById("galleryMessage");
			if (failed) {
				setMessage(galleryMsg, "Delete failed - admin role required.", true);
				return;
			}
			removeItemFromState(item);
			renderGrid();
			setMessage(document.getElementById("galleryMessage"), "Deleted.", false);
		}

		paths.forEach(function(path) {
			fetch(path, { method: "DELETE" }).then(function(response) {
				if (!response.ok && response.status !== 404)
					failed = true;
				done();
			}).catch(function() {
				failed = true;
				done();
			});
		});
	}

	function buildItemTile(item) {
		var card = document.createElement("div");
		card.className = "gallery-card";

		var link = document.createElement("a");
		link.href = item.page;
		link.className = "gallery-card-link";

		var img = document.createElement("img");
		img.src = item.image;
		img.alt = "";
		link.appendChild(img);

		var title = document.createElement("span");
		title.className = "gallery-card-title";
		title.textContent = item.title;
		link.appendChild(title);

		card.appendChild(link);

		if (isLoggedIn()) {
			var del = document.createElement("button");
			del.type = "button";
			del.className = "gallery-card-delete icon solid fa-solid fa-trash";
			del.setAttribute("aria-label", "Delete " + item.title);
			del.addEventListener("click", function(event) {
				event.preventDefault();
				deleteItem(item);
			});
			card.appendChild(del);
		}

		return card;
	}

	function handleUpload(fileInput, titleInput, descInput, msgEl) {
		var file = fileInput.files[0];
		var title = titleInput.value.trim();
		var description = descInput.value.trim();
		if (!file || !title || !description)
			return;

		var safeName = Date.now() + "-" + file.name.replace(/[^a-zA-Z0-9._-]/g, "_");
		var dotIndex = safeName.lastIndexOf(".");
		var baseName = dotIndex > -1 ? safeName.substring(0, dotIndex) : safeName;
		var pageName = baseName + ".html";
		var imagePath = "/uploads/" + safeName;
		var pagePath = "/uploads/" + pageName;

		setMessage(msgEl, "Uploading...", false);

		fetch(imagePath, {
			method: "POST",
			headers: { "Content-Type": file.type || "application/octet-stream" },
			body: file
		}).then(function(response) {
			if (!response.ok)
				throw new Error("image upload failed (" + response.status + ")");

			return fetch(pagePath, {
				method: "POST",
				headers: { "Content-Type": "text/html" },
				body: buildCompanionPage(title, description, imagePath)
			});
		}).then(function(response) {
			if (!response.ok)
				throw new Error("page upload failed (" + response.status + ")");

			items.unshift({
				image: imagePath,
				page: pagePath,
				title: title,
				deletable: [ imagePath, pagePath ]
			});
			currentPage = 1;
			renderGrid();
		}).catch(function(err) {
			setMessage(msgEl, "Upload failed - " + err.message, true);
		});
	}

	function buildUploadTile() {
		var card = document.createElement("div");
		card.className = "gallery-card upload-card";

		var loggedIn = isLoggedIn();

		var locked = document.createElement("div");
		locked.className = "upload-card-state";
		locked.hidden = loggedIn;
		var lockIcon = document.createElement("span");
		lockIcon.className = "icon solid fa-lock";
		var lockText = document.createElement("p");
		lockText.textContent = "Log in to upload an image.";
		locked.appendChild(lockIcon);
		locked.appendChild(lockText);
		card.appendChild(locked);

		var form = document.createElement("form");
		form.id = "uploadForm";
		form.className = "upload-card-state";
		form.hidden = !loggedIn;

		var uploadIcon = document.createElement("span");
		uploadIcon.className = "icon solid fa-upload";
		form.appendChild(uploadIcon);

		var titleInput = document.createElement("input");
		titleInput.type = "text";
		titleInput.id = "uploadTitle";
		titleInput.placeholder = "Title";
		titleInput.required = true;
		form.appendChild(titleInput);

		var descInput = document.createElement("textarea");
		descInput.id = "uploadDescription";
		descInput.placeholder = "Description";
		descInput.required = true;
		form.appendChild(descInput);

		var actions = document.createElement("div");
		actions.className = "upload-actions";

		var fileInput = document.createElement("input");
		fileInput.type = "file";
		fileInput.id = "uploadFile";
		fileInput.accept = "image/*";
		fileInput.required = true;

		var fileLabel = document.createElement("label");
		fileLabel.setAttribute("for", "uploadFile");
		fileLabel.id = "uploadFileLabel";
		fileLabel.className = "button button-compact";
		fileLabel.textContent = "Browse";

		fileInput.addEventListener("change", function() {
			fileLabel.classList.toggle("has-file", fileInput.files.length > 0);
		});

		var submitBtn = document.createElement("button");
		submitBtn.type = "submit";
		submitBtn.className = "button button-compact";
		submitBtn.textContent = "Upload";

		actions.appendChild(fileLabel);
		actions.appendChild(submitBtn);
		form.appendChild(fileInput);
		form.appendChild(actions);

		var msg = document.createElement("p");
		msg.id = "uploadMessage";
		form.appendChild(msg);

		form.addEventListener("submit", function(event) {
			event.preventDefault();
			handleUpload(fileInput, titleInput, descInput, msg);
		});

		card.appendChild(form);

		return card;
	}

	function renderPager(pages) {
		var pager = document.getElementById("galleryPager");
		var indicator = document.getElementById("galleryPageIndicator");
		if (!pager)
			return;
		pager.hidden = pages <= 1;
		if (indicator)
			indicator.textContent = currentPage + " / " + pages;
	}

	function renderGrid() {
		var grid = document.getElementById("galleryGrid");
		if (!grid)
			return;

		var tiles = combinedTiles();
		var pages = totalPages();
		if (currentPage > pages)
			currentPage = pages;
		if (currentPage < 1)
			currentPage = 1;

		var start = (currentPage - 1) * PAGE_SIZE;
		var pageTiles = tiles.slice(start, start + PAGE_SIZE);

		grid.textContent = "";
		pageTiles.forEach(function(tile) {
			grid.appendChild(tile.type === "item" ? buildItemTile(tile.data) : buildUploadTile());
		});

		renderPager(pages);
	}

	document.addEventListener("DOMContentLoaded", function() {
		loadItems().catch(function() {
			// listing unavailable - fall back to just the upload tile
		}).then(function() {
			renderGrid();
		});

		var prev = document.getElementById("galleryPrev");
		if (prev) {
			prev.addEventListener("click", function() {
				currentPage--;
				renderGrid();
			});
		}

		var next = document.getElementById("galleryNext");
		if (next) {
			next.addEventListener("click", function() {
				currentPage++;
				renderGrid();
			});
		}

		document.addEventListener("webserv:authchange", function() {
			renderGrid();
		});
	});

})();
