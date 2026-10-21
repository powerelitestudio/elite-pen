#include "board_recognition.hpp"
#include <limits>
#include <msinkaut.h>
#include <stdexcept>
#include <windows.h>
#include <wrl/client.h>

namespace elite_pen::win {
namespace {
using Microsoft::WRL::ComPtr;
struct Value : VARIANT {
    Value() { VariantInit(this); }
    ~Value() { VariantClear(this); }
    Value(const Value &) = delete;
};
HRESULT invoke(IDispatch *object, const wchar_t *name, WORD flags, VARIANT *result,
               VARIANT *arguments = nullptr, UINT count = 0) {
    DISPID id{};
    auto *member = const_cast<wchar_t *>(name);
    HRESULT hr = object->GetIDsOfNames(IID_NULL, &member, 1, LOCALE_USER_DEFAULT, &id);
    if (FAILED(hr))
        return hr;
    DISPID put = DISPID_PROPERTYPUT;
    const bool setter = (flags & (DISPATCH_PROPERTYPUT | DISPATCH_PROPERTYPUTREF)) != 0;
    DISPPARAMS params{arguments, setter ? &put : nullptr, count, setter ? 1U : 0U};
    EXCEPINFO exception{};
    hr = object->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, flags, &params, result, &exception, nullptr);
    if (exception.pfnDeferredFillIn)
        exception.pfnDeferredFillIn(&exception);
    SysFreeString(exception.bstrSource);
    SysFreeString(exception.bstrDescription);
    SysFreeString(exception.bstrHelpFile);
    return hr;
}
ComPtr<IDispatch> create(const wchar_t *name) {
    CLSID clsid{};
    ComPtr<IDispatch> result;
    if (SUCCEEDED(CLSIDFromProgID(name, &clsid)))
        CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(result.GetAddressOf()));
    return result;
}
BoardRecognition run(const std::vector<std::vector<PointF>> &strokes) {
    BoardRecognition result;
    auto recognizers = create(L"msinkaut.InkRecognizers");
    auto collector = create(L"msinkaut.InkCollector");
    Value dispatch;
    if (collector)
        invoke(collector.Get(), L"Ink", DISPATCH_PROPERTYGET, &dispatch);
    ComPtr<IInkDisp> ink;
    if (!recognizers || dispatch.vt != VT_DISPATCH || !dispatch.pdispVal ||
        FAILED(dispatch.pdispVal->QueryInterface(IID_PPV_ARGS(ink.GetAddressOf())))) {
        result.error = L"Windows no tiene un reconocedor de escritura disponible.";
        return result;
    }
    Value count;
    if (FAILED(invoke(recognizers.Get(), L"Count", DISPATCH_PROPERTYGET, &count)) || count.vt != VT_I4)
        return {L"", L"", L"No se pudo consultar el reconocimiento de Windows."};
    ComPtr<IDispatch> context;
    int best = -1;
    const LANGID wanted = GetUserDefaultLangID();
    for (LONG n = 0; n < count.lVal; ++n) {
        Value index, item, languages, name;
        index.vt = VT_I4;
        index.lVal = n;
        if (FAILED(invoke(recognizers.Get(), L"Item", DISPATCH_METHOD | DISPATCH_PROPERTYGET, &item, &index,
                          1)) ||
            item.vt != VT_DISPATCH || !item.pdispVal)
            continue;
        if (FAILED(invoke(item.pdispVal, L"Languages", DISPATCH_PROPERTYGET, &languages)) ||
            !(languages.vt & VT_ARRAY) || !languages.parray)
            continue;
        LONG first{}, last{-1};
        SafeArrayGetLBound(languages.parray, 1, &first);
        SafeArrayGetUBound(languages.parray, 1, &last);
        int score = -1;
        for (LONG l = first; l <= last; ++l) {
            LONG lang{};
            if (SUCCEEDED(SafeArrayGetElement(languages.parray, &l, &lang)))
                score = std::max(score,
                                 lang == wanted ? 3 : (PRIMARYLANGID(lang) == PRIMARYLANGID(wanted) ? 2 : 0));
        }
        if (score <= best)
            continue; // Gesture recognizers have no text languages.
        Value candidate;
        if (FAILED(invoke(item.pdispVal, L"CreateRecognizerContext", DISPATCH_METHOD, &candidate)) ||
            candidate.vt != VT_DISPATCH || !candidate.pdispVal)
            continue;
        candidate.pdispVal->QueryInterface(IID_PPV_ARGS(context.ReleaseAndGetAddressOf()));
        if (!context)
            continue;
        best = score;
        if (SUCCEEDED(invoke(item.pdispVal, L"Name", DISPATCH_PROPERTYGET, &name)) && name.vt == VT_BSTR)
            result.recognizer = name.bstrVal;
    }
    if (!context)
        return {L"", L"", L"Instala un reconocedor de escritura en los idiomas de Windows."};
    PointF origin{std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    std::size_t total{};
    for (const auto &stroke : strokes)
        for (const auto p : stroke) {
            if (!std::isfinite(p.x) || !std::isfinite(p.y))
                return {L"", L"", L"Trazo no válido."};
            origin.x = std::min(origin.x, p.x);
            origin.y = std::min(origin.y, p.y);
            if (++total > 100000)
                return {L"", L"", L"Convierte fragmentos de escritura más pequeños."};
        }
    if (!total)
        return result;
    for (const auto &stroke : strokes) {
        if (stroke.empty())
            continue;
        Value packets, description;
        packets.vt = VT_ARRAY | VT_I4;
        const auto length = std::max<std::size_t>(2, stroke.size());
        packets.parray = SafeArrayCreateVector(VT_I4, 0, static_cast<ULONG>(length * 2));
        LONG *data{};
        if (!packets.parray || FAILED(SafeArrayAccessData(packets.parray, reinterpret_cast<void **>(&data))))
            return {L"", L"", L"No se pudo preparar la escritura."};
        for (std::size_t n = 0; n < length; ++n) {
            const auto p = stroke[std::min(n, stroke.size() - 1)];
            data[2 * n] = static_cast<LONG>(std::clamp((p.x - origin.x) * (2540.0 / 96), 0.0, 100000000.0));
            data[2 * n + 1] =
                static_cast<LONG>(std::clamp((p.y - origin.y) * (2540.0 / 96), 0.0, 100000000.0));
        }
        SafeArrayUnaccessData(packets.parray);
        ComPtr<IInkStrokeDisp> added;
        if (FAILED(ink->CreateStroke(packets, description, added.GetAddressOf())))
            return {L"", L"", L"Windows no pudo leer los trazos."};
    }
    ComPtr<IInkStrokes> collection;
    if (FAILED(ink->get_Strokes(collection.GetAddressOf())))
        return result;
    Value argument;
    argument.vt = VT_DISPATCH;
    if (FAILED(collection->QueryInterface(IID_IDispatch, reinterpret_cast<void **>(&argument.pdispVal))))
        return result;
    if (FAILED(invoke(context.Get(), L"Strokes", DISPATCH_PROPERTYPUTREF, nullptr, &argument, 1)))
        return {L"", L"", L"No se pudo enviar la escritura al reconocedor."};
    LONG status{};
    Value status_arg, recognized, text;
    status_arg.vt = VT_I4 | VT_BYREF;
    status_arg.plVal = &status;
    if (FAILED(invoke(context.Get(), L"Recognize", DISPATCH_METHOD, &recognized, &status_arg, 1)) ||
        recognized.vt != VT_DISPATCH || !recognized.pdispVal ||
        FAILED(invoke(recognized.pdispVal, L"TopString", DISPATCH_PROPERTYGET, &text)) || text.vt != VT_BSTR)
        return {L"", L"", L"No se reconoció texto; se conserva tu escritura."};
    if (text.bstrVal)
        result.text.assign(text.bstrVal, SysStringLen(text.bstrVal));
    if (result.text.size() > 100000)
        result.text.clear();
    return result;
}
} // namespace
BoardRecognition recognize_board_ink(const std::vector<std::vector<PointF>> &strokes) {
    const HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(apartment))
        return {L"", L"", L"No se pudo iniciar el reconocimiento local."};
    BoardRecognition result;
    try {
        result = run(strokes);
    } catch (...) {
        result.error = L"No se pudo reconocer; se conserva tu escritura.";
    }
    CoUninitialize(); // All recognizer objects were destroyed inside run().
    return result;
}
} // namespace elite_pen::win
