/* Readable UI helpers; disclosure controls themselves use native HTML. */
"use strict";

function addCopyButtons() {
    document.querySelectorAll("pre > code").forEach((code) => {
        if (code.parentElement.parentElement.querySelector(".md-clipboard, .copy-code")) {
            return;
        }
        const button = document.createElement("button");
        button.type = "button";
        button.className = "copy-code";
        button.textContent = "Copy";
        button.setAttribute("aria-label", "Copy code");
        button.addEventListener("click", async () => {
            try {
                await navigator.clipboard.writeText(code.textContent);
                button.textContent = "Copied";
            } catch {
                button.textContent = "Select code to copy";
            }
        });
        code.parentElement.appendChild(button);
    });
}

if (document.readyState === "complete") {
    addCopyButtons();
} else {
    window.addEventListener("load", addCopyButtons, {
        once: true
    });
}
