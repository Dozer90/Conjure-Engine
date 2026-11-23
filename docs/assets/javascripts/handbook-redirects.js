/* Preserve bookmarks and queries when a handbook page or section moves. */
(() => {
    const redirect = document.querySelector('meta[name="doc-redirect"]');
    if (!redirect) {
        return;
    }

    const fragments = JSON.parse(
        document.querySelector('meta[name="doc-redirect-fragments"]')?.content || "{}"
    );
    const rawAnchor = window.location.hash.slice(1);
    let anchor;
    try {
        anchor = decodeURIComponent(rawAnchor);
    } catch {
        anchor = rawAnchor;
    }
    const target = new URL(fragments[anchor] || redirect.content, window.location.href);
    if (target.origin !== window.location.origin) {
        return;
    }
    if (!target.hash) {
        target.hash = window.location.hash;
    }
    target.search = window.location.search;
    window.location.replace(target.href);
})();
