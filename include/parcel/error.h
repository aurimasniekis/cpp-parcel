#pragma once

/**
 * @file error.h
 * @brief `ParcelError` and the non-throwing surface that returns `std::expected`.
 *
 * Throwing `from_json` and registry lookups remain the default. Callers that
 * prefer a value-typed error path (a JSON parse failing for an external
 * input, for instance) get the parallel `try_*` API surface that returns
 * `std::expected<cell_t, ParcelError>` instead.
 */

#include <stdexcept>
#include <string>
#include <utility>

// `<expected>` ships in libstdc++ 14 / libc++ 17, but only enables the
// `std::expected` templates when the compiler meets internal concept /
// constexpr gates. Include the header first (`__has_include` only checks
// for file existence) and then check `__cpp_lib_expected` — that macro is
// defined by `<expected>` itself iff the templates were actually exposed,
// so it stays consistent across g++ and clang-tidy parse paths.
#if __has_include(<expected>)
#include <expected>
#endif

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#define PARCEL_HAS_EXPECTED 1
#else
#define PARCEL_HAS_EXPECTED 0
#endif

namespace parcel {

/**
 * @brief Structured error returned by the non-throwing `try_*` parsing surface.
 *
 * Carries a coarse code, a human-readable message, and (when relevant) the
 * cell kind id and field name involved.
 */
struct ParcelError {
    /** @brief Coarse error categories. */
    enum class Code {
        /** @brief Input was not parseable JSON or had wrong shape. */
        InvalidJson,
        /** @brief JSON `"k"` did not match the expected kind. */
        KindMismatch,
        /** @brief JSON `"k"` referenced a kind not registered. */
        UnknownKind,
        /** @brief A required struct field was missing. */
        MissingField,
        /** @brief A typed value (e.g. struct field) failed conversion. */
        TypeError,
    };

    /** @brief Coarse error code. */
    Code code{Code::InvalidJson};
    /** @brief Human-readable message — never empty. */
    std::string message;
    /** @brief Kind id involved in the error (may be empty). */
    std::string kind;
    /** @brief Struct field name involved in the error (may be empty). */
    std::string field;

    /**
     * @brief Render the error as `code: message [kind=…] [field=…]`.
     * @return Compact diagnostic string.
     */
    [[nodiscard]] std::string to_string() const {
        std::string out;
        switch (code) {
        case Code::InvalidJson:
            out = "InvalidJson";
            break;
        case Code::KindMismatch:
            out = "KindMismatch";
            break;
        case Code::UnknownKind:
            out = "UnknownKind";
            break;
        case Code::MissingField:
            out = "MissingField";
            break;
        case Code::TypeError:
            out = "TypeError";
            break;
        }
        out += ": ";
        out += message;
        if (!kind.empty()) {
            out += " [kind=";
            out += kind;
            out += "]";
        }
        if (!field.empty()) {
            out += " [field=";
            out += field;
            out += "]";
        }
        return out;
    }

    /**
     * @brief Construct from code, message, and optional kind/field tags.
     */
    static ParcelError
    make(const Code c, std::string msg, std::string kind_id = {}, std::string fld = {}) {
        return ParcelError{c, std::move(msg), std::move(kind_id), std::move(fld)};
    }
};

/**
 * @brief Base class for parcel deserialization exceptions.
 *
 * Inherits `std::runtime_error` so existing `catch(std::runtime_error&)` and
 * `EXPECT_THROW(..., std::runtime_error)` continue to work; adds a structured
 * `ParcelError::Code` plus optional `kind` / `field` tags so the
 * non-throwing `try_*` API can faithfully translate the exception into a
 * `ParcelError` without losing granularity.
 */
class ParcelException : public std::runtime_error {
public:
    // std::runtime_error only takes a const-ref string, so msg is necessarily
    // copied into the base sub-object — pass-by-value-then-move would buy us
    // nothing here. Adjacent string params are intentional and used by every
    // derived class.
    // NOLINTBEGIN(bugprone-easily-swappable-parameters,performance-unnecessary-value-param)
    ParcelException(const ParcelError::Code c,
                    const std::string& msg,
                    std::string kind_id = {},
                    std::string field_name = {})
        : std::runtime_error(msg), code_(c), kind_(std::move(kind_id)),
          field_(std::move(field_name)) {}
    // NOLINTEND(bugprone-easily-swappable-parameters,performance-unnecessary-value-param)

    [[nodiscard]] ParcelError::Code code() const noexcept {
        return code_;
    }
    [[nodiscard]] std::string_view kind_id() const noexcept {
        return kind_;
    }
    [[nodiscard]] std::string_view field_name() const noexcept {
        return field_;
    }

    /** @brief Project to a `ParcelError` value (used by `try_*` adapters). */
    [[nodiscard]] ParcelError to_error() const {
        return ParcelError::make(code_, what(), kind_, field_);
    }

private:
    ParcelError::Code code_;
    std::string kind_;
    std::string field_;
};

/** @brief JSON shape was wrong (missing/non-string `k`, missing `v`, etc.). */
class InvalidJsonException final : public ParcelException {
public:
    explicit InvalidJsonException(const std::string& msg,
                                  std::string kind_id = {},
                                  std::string field_name = {})
        : ParcelException(
              ParcelError::Code::InvalidJson, msg, std::move(kind_id), std::move(field_name)) {}
};

/** @brief `k` field was present but did not match the expected kind. */
class KindMismatchException final : public ParcelException {
public:
    explicit KindMismatchException(const std::string& msg, std::string kind_id = {})
        : ParcelException(ParcelError::Code::KindMismatch, msg, std::move(kind_id)) {}
};

/** @brief Registry was asked to dispatch a kind it does not know. */
class UnknownKindException final : public ParcelException {
public:
    explicit UnknownKindException(const std::string& msg, std::string kind_id = {})
        : ParcelException(ParcelError::Code::UnknownKind, msg, std::move(kind_id)) {}
};

/** @brief Required struct field absent from the JSON payload. */
class MissingFieldException final : public ParcelException {
public:
    MissingFieldException(const std::string& msg, std::string kind_id, std::string field_name)
        : ParcelException(
              ParcelError::Code::MissingField, msg, std::move(kind_id), std::move(field_name)) {}
};

/** @brief Typed value (cast, struct field, etc.) failed conversion. */
class TypeException final : public ParcelException {
public:
    explicit TypeException(const std::string& msg,
                           std::string kind_id = {},
                           std::string field_name = {})
        : ParcelException(
              ParcelError::Code::TypeError, msg, std::move(kind_id), std::move(field_name)) {}
};

}  // namespace parcel
