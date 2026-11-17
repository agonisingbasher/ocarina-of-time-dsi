/* Motor de idioma das páginas secundárias do site (site-development.html,
   support.html). A página define window.pageTranslations com as chaves
   "pt-BR", "en", "es" e "fr" antes de carregar este arquivo.

   Precedência: ?lang= > escolha gravada > idioma do navegador > pt-BR.
   A chave de armazenamento é a mesma do resto dos sites, então trocar o
   idioma aqui troca em qualquer página do autor. */
(function () {
  "use strict";

  const LANGUAGES = ["pt-BR", "en", "es", "fr"];
  const STORAGE_KEY = "agonisingbasher-language";
  const translations = window.pageTranslations || {};
  const buttons = document.querySelectorAll("[data-lang-option]");
  const meta = document.querySelector('meta[name="description"]');

  function value(language, key) {
    const content = translations[language] || {};
    return Object.prototype.hasOwnProperty.call(content, key) ? content[key] : null;
  }

  function setLanguage(language) {
    if (LANGUAGES.indexOf(language) < 0 || !translations[language]) return;
    document.documentElement.lang = language;

    const title = value(language, "titulo");
    if (title) document.title = title;

    const description = value(language, "descricao");
    if (description && meta) meta.setAttribute("content", description);

    document.querySelectorAll("[data-i18n]").forEach((element) => {
      const text = value(language, element.dataset.i18n);
      if (text !== null) element.innerHTML = text;
    });

    document.querySelectorAll("[data-i18n-aria-label]").forEach((element) => {
      const text = value(language, element.dataset.i18nAriaLabel);
      if (text !== null) element.setAttribute("aria-label", text);
    });

    document.querySelectorAll("[data-i18n-alt]").forEach((element) => {
      const text = value(language, element.dataset.i18nAlt);
      if (text !== null) element.alt = text;
    });

    buttons.forEach((button) => {
      button.setAttribute("aria-pressed", String(button.dataset.langOption === language));
    });

    document.dispatchEvent(new CustomEvent("page-language-change", { detail: { language } }));
  }

  function detectLanguage() {
    const preferred = navigator.languages && navigator.languages.length ? navigator.languages : [navigator.language];
    for (const tag of preferred) {
      if (!tag) continue;
      const base = String(tag).toLowerCase().split("-")[0];
      if (base === "pt") return "pt-BR";
      const match = LANGUAGES.find((code) => code.toLowerCase() === base);
      if (match) return match;
    }
    return "pt-BR";
  }

  function readStoredLanguage() {
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      return LANGUAGES.indexOf(stored) >= 0 ? stored : null;
    } catch (error) {
      return null;
    }
  }

  buttons.forEach((button) => {
    button.addEventListener("click", () => {
      const language = button.dataset.langOption;
      setLanguage(language);
      try {
        localStorage.setItem(STORAGE_KEY, language);
      } catch (error) {
        /* armazenamento bloqueado: o idioma vale para esta leitura, não para a próxima */
      }
    });
  });

  const requested = new URLSearchParams(window.location.search).get("lang");
  const initial = LANGUAGES.indexOf(requested) >= 0 ? requested : readStoredLanguage() || detectLanguage();
  setLanguage(initial);
})();
