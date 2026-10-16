#pragma once

#include <array>
#include <cstddef>

namespace elite_pen {

struct EmojiInfo {
    const wchar_t* glyph;
    const wchar_t* name;
};

// IDs are persisted in settings.ini. Append entries; never reorder existing IDs.
// Use the Windows color font; no downloaded assets, fonts or network access.
inline constexpr std::array<EmojiInfo, 72> kEmojis{{
    {L"\U0001F44D", L"Bien"}, {L"\u2705", L"Correcto"},
    {L"\u274C", L"Error"}, {L"\u26A0\uFE0F", L"Advertencia"},
    {L"\U0001F6AB", L"Prohibido"}, {L"\U0001F4A1", L"Idea"},
    {L"\U0001F3AF", L"Objetivo"}, {L"\u2764\uFE0F", L"Corazón"},
    {L"\u2753", L"Pregunta"}, {L"\u2757", L"Importante"},
    {L"\u2B50", L"Estrella"}, {L"\U0001F525", L"Fuego"},
    {L"\U0001F4AF", L"Cien"}, {L"\U0001F389", L"Celebración"},
    {L"\U0001F680", L"Despegue"}, {L"\U0001F512", L"Bloqueado"},
    {L"\U0001F513", L"Desbloqueado"}, {L"\U0001F6D1", L"Detener"},
    {L"\U0001F534", L"Rojo"}, {L"\U0001F7E2", L"Verde"},
    {L"\U0001F535", L"Azul"}, {L"\U0001F7E1", L"Amarillo"},
    {L"\u23F3", L"En proceso"}, {L"\u231B", L"Tiempo agotado"},
    {L"\U0001F600", L"Sonrisa"}, {L"\U0001F603", L"Alegría"},
    {L"\U0001F604", L"Muy feliz"}, {L"\U0001F601", L"Gran sonrisa"},
    {L"\U0001F602", L"Risa"}, {L"\U0001F923", L"Carcajada"},
    {L"\U0001F609", L"Guiño"}, {L"\U0001F60E", L"Genial"},
    {L"\U0001F914", L"Pensando"}, {L"\U0001F928", L"Duda"},
    {L"\U0001F62E", L"Sorpresa"}, {L"\U0001F631", L"Asombro"},
    {L"\U0001F610", L"Neutral"}, {L"\U0001F615", L"Confusión"},
    {L"\U0001F622", L"Tristeza"}, {L"\U0001F62D", L"Llanto"},
    {L"\U0001F621", L"Enojo"}, {L"\U0001F634", L"Dormido"},
    {L"\U0001F973", L"Fiesta"}, {L"\U0001F929", L"Entusiasmo"},
    {L"\U0001F60D", L"Me encanta"}, {L"\U0001F607", L"Inocente"},
    {L"\U0001F913", L"Estudioso"}, {L"\U0001F917", L"Abrazo"},
    {L"\U0001F44E", L"Mal"}, {L"\U0001F44F", L"Aplausos"},
    {L"\U0001F64C", L"Logro"}, {L"\U0001F64F", L"Gracias"},
    {L"\U0001F44C", L"Perfecto"}, {L"\u270C\uFE0F", L"Victoria"},
    {L"\U0001F91D", L"Acuerdo"}, {L"\U0001F4AA", L"Fuerza"},
    {L"\U0001F448", L"Izquierda"}, {L"\U0001F449", L"Derecha"},
    {L"\U0001F446", L"Arriba"}, {L"\U0001F447", L"Abajo"},
    {L"\U0001F4DA", L"Libros"}, {L"\U0001F4DD", L"Nota"},
    {L"\U0001F4CC", L"Recordatorio"}, {L"\U0001F50D", L"Revisar"},
    {L"\U0001F4CA", L"Gráfico"}, {L"\U0001F4C8", L"Crecimiento"},
    {L"\U0001F4C9", L"Descenso"}, {L"\U0001F3C6", L"Premio"},
    {L"\u23F0", L"Alarma"}, {L"\u2615", L"Pausa"},
    {L"\U0001F4BB", L"Computador"}, {L"\U0001F393", L"Aprendizaje"}
}};

inline constexpr std::size_t kEmojiFavoriteCount = 8;
inline constexpr std::size_t kEmojisPerPage = 24;
inline constexpr std::array<const wchar_t*, 3> kEmojiPageNames{
    L"Símbolos", L"Caras", L"Gestos y clase"};
inline constexpr std::array<int, kEmojiFavoriteCount> kDefaultEmojiFavorites{
    0, 1, 2, 3, 4, 5, 6, 7};
static_assert(kEmojis.size() == kEmojiPageNames.size() * kEmojisPerPage);

[[nodiscard]] inline constexpr bool valid_emoji(int id) noexcept {
    return id >= 0 && static_cast<std::size_t>(id) < kEmojis.size();
}

}  // namespace elite_pen
