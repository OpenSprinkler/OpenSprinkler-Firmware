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
});
