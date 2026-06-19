// ZAKO — Sovereign Mobile Operating System
// Multi-article navigation, annotation toggles, scrollspy.

(function () {
  "use strict";

  var articles = document.querySelectorAll(".article-page");
  var navLinks = document.querySelectorAll(".nav-link");
  var tocContainer = document.getElementById("toc-sections");

  function showArticle(id) {
    articles.forEach(function (a) { a.hidden = (a.id !== id); });
    navLinks.forEach(function (link) { link.classList.toggle("active", link.dataset.article === id); });
    buildTOC(id);
    window.scrollTo(0, 0);
    initAnnotations();
    initScrollspy();
  }

  navLinks.forEach(function (link) {
    link.addEventListener("click", function (e) {
      e.preventDefault();
      showArticle(link.dataset.article);
    });
  });

  function buildTOC(articleId) {
    var article = document.getElementById(articleId);
    if (!article || !tocContainer) return;
    var headings = article.querySelectorAll("h2, h3");
    var html = "";
    headings.forEach(function (h, i) {
      var id = h.id || "heading-" + articleId + "-" + i;
      h.id = id;
      var level = h.tagName === "H3" ? "toc-sub" : "toc-main";
      var text = h.textContent.replace(/^[\d]+\s*/, "").replace(/^—\s*/, "");
      html += '<a href="#' + id + '" class="toc-link ' + level + '">' + text + '</a>';
    });
    tocContainer.innerHTML = html;
  }

  function initAnnotations() {
    document.querySelectorAll(".note-ref").forEach(function (ref) {
      var newRef = ref.cloneNode(true);
      ref.parentNode.replaceChild(newRef, ref);
      newRef.addEventListener("click", function () {
        var note = document.getElementById(newRef.dataset.note);
        if (!note) return;
        var open = !note.hidden;
        note.hidden = open;
        newRef.setAttribute("aria-expanded", String(!open));
        if (!open) { note.style.animation = "none"; void note.offsetWidth; note.style.animation = ""; }
      });
    });
  }

  function initScrollspy() {
    var tocLinks = Array.prototype.slice.call(tocContainer.querySelectorAll(".toc-link"));
    if (!tocLinks.length) return;
    var currentToc = null;
    function onScroll() {
      var marker = window.innerHeight * 0.3;
      var activeId = null;
      tocLinks.forEach(function (link) {
        var target = document.getElementById(link.getAttribute("href").slice(1));
        if (target && target.getBoundingClientRect().top <= marker) activeId = link.getAttribute("href").slice(1);
      });
      if (activeId !== currentToc) {
        currentToc = activeId;
        tocLinks.forEach(function (l) { l.classList.remove("active"); });
        var activeLink = tocContainer.querySelector('a[href="#' + activeId + '"]');
        if (activeLink) activeLink.classList.add("active");
      }
    }
    window.removeEventListener("scroll", onScroll);
    window.addEventListener("scroll", onScroll, { passive: true });
    onScroll();
  }

  showArticle("part1");
})();
