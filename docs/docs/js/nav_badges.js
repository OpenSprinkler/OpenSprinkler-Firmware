document.addEventListener("DOMContentLoaded", function () {
  document.querySelectorAll(".wy-menu-vertical .caption-text").forEach(function (label) {
    var marker = "[NEW]";
    var text = label.textContent.trim();
    if (!text.endsWith(marker)) return;

    label.textContent = text.slice(0, -marker.length).trimEnd();

    var badge = document.createElement("span");
    badge.className = "nav-new-badge";
    badge.textContent = "NEW";
    badge.setAttribute("aria-label", "New");
    label.appendChild(badge);
  });

  // MkDocs emits title-only navigation groups with href="#" or without href,
  // depending on its version. Wait for the theme's expand buttons, then
  // restore the actual page as the sole current branch.
  window.setTimeout(function fixNavigation() {
    var menu = document.querySelector(".wy-menu-vertical");
    if (!menu) return;

    var groupLinks = menu.querySelectorAll('a.reference.internal[href="#"], a.reference.internal:not([href])');
    if (groupLinks.length && !Array.from(groupLinks).every(function (link) {
      return link.querySelector(".toctree-expand");
    })) {
      window.setTimeout(fixNavigation, 10);
      return;
    }

    groupLinks.forEach(function (link) {
      link.addEventListener("click", function (event) {
        if (event.target.closest(".toctree-expand")) return;

        event.preventDefault();
        var button = link.querySelector(".toctree-expand");
        if (button) button.click();
      });
    });

    if (window.location.hash) return;

    var currentPath = window.location.pathname.replace(/\/index\.html$/, "").replace(/\/$/, "");
    var currentLink = Array.from(menu.querySelectorAll('a.reference.internal[href]:not([href="#"])')).find(function (link) {
      var url = new URL(link.href, window.location.href);
      var linkPath = url.pathname.replace(/\/index\.html$/, "").replace(/\/$/, "");
      return !url.hash && linkPath === currentPath;
    });

    if (!currentLink) return;

    menu.querySelectorAll(".current").forEach(function (element) {
      element.classList.remove("current");
      element.setAttribute("aria-expanded", "false");
    });

    currentLink.classList.add("current");
    currentLink.setAttribute("aria-expanded", "true");

    var parent = currentLink.parentElement;
    while (parent && parent !== menu) {
      if (parent.matches("li, ul")) {
        parent.classList.add("current");
        parent.setAttribute("aria-expanded", "true");
      }
      if (parent.matches("li")) {
        var branchLink = parent.querySelector(":scope > a");
        if (branchLink) branchLink.setAttribute("aria-expanded", "true");
      }
      parent = parent.parentElement;
    }
  }, 0);
});
