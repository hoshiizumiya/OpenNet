/** Native-only attached helpers must not be copied onto wrapper controls. */
export function removeIncompatibleGlassHelpers(tag, kind) {
    if (kind !== 'SearchBox') return tag;
    return tag.replace(/\s+[\w.-]+:AutoSuggestBoxHelper\.[\w]+\s*=\s*"[^"]*"/g, '');
}
