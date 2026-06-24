addToLibrary({
    dom_set_innertext: function(target, inner_text) {
        let js_target = UTF8ToString(target);
        let js_inner_text = UTF8ToString(inner_text);
        document.querySelector(js_target).textContent = js_inner_text;
    }
})