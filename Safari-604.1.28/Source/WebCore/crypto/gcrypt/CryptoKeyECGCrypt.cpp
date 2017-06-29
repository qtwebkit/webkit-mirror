/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CryptoKeyEC.h"

#if ENABLE(SUBTLE_CRYPTO)

#include "CryptoKeyPair.h"
#include "GCryptUtilities.h"
#include "JsonWebKey.h"
#include "NotImplemented.h"
#include <array>
#include <pal/crypto/gcrypt/Handle.h>
#include <pal/crypto/gcrypt/Utilities.h>
#include <pal/crypto/tasn1/Utilities.h>
#include <wtf/text/Base64.h>

namespace WebCore {

static size_t curveSize(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return 256;
    case CryptoKeyEC::NamedCurve::P384:
        return 384;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static const char* curveName(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return "NIST P-256";
    case CryptoKeyEC::NamedCurve::P384:
        return "NIST P-384";
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

static unsigned uncompressedPointSizeForCurve(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return 65;
    case CryptoKeyEC::NamedCurve::P384:
        return 97;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

static unsigned uncompressedFieldElementSizeForCurve(CryptoKeyEC::NamedCurve curve)
{
    switch (curve) {
    case CryptoKeyEC::NamedCurve::P256:
        return 32;
    case CryptoKeyEC::NamedCurve::P384:
        return 48;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

CryptoKeyEC::~CryptoKeyEC()
{
    if (m_platformKey)
        PAL::GCrypt::HandleDeleter<gcry_sexp_t>()(m_platformKey);
}

size_t CryptoKeyEC::keySizeInBits() const
{
    size_t size = curveSize(m_curve);
    ASSERT(size == gcry_pk_get_nbits(m_platformKey));
    return size;
}

std::optional<CryptoKeyPair> CryptoKeyEC::platformGeneratePair(CryptoAlgorithmIdentifier identifier, NamedCurve curve, bool extractable, CryptoKeyUsageBitmap usages)
{
    PAL::GCrypt::Handle<gcry_sexp_t> genkeySexp;
    gcry_error_t error = gcry_sexp_build(&genkeySexp, nullptr, "(genkey(ecc(curve %s)))", curveName(curve));
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    PAL::GCrypt::Handle<gcry_sexp_t> keyPairSexp;
    error = gcry_pk_genkey(&keyPairSexp, genkeySexp);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return std::nullopt;
    }

    PAL::GCrypt::Handle<gcry_sexp_t> publicKeySexp(gcry_sexp_find_token(keyPairSexp, "public-key", 0));
    PAL::GCrypt::Handle<gcry_sexp_t> privateKeySexp(gcry_sexp_find_token(keyPairSexp, "private-key", 0));
    if (!publicKeySexp || !privateKeySexp)
        return std::nullopt;

    auto publicKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Public, publicKeySexp.release(), true, usages);
    auto privateKey = CryptoKeyEC::create(identifier, curve, CryptoKeyType::Private, privateKeySexp.release(), extractable, usages);
    return CryptoKeyPair { WTFMove(publicKey), WTFMove(privateKey) };
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportRaw(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    if (keyData.size() != uncompressedPointSizeForCurve(curve))
        return nullptr;

    PAL::GCrypt::Handle<gcry_sexp_t> platformKey;
    gcry_error_t error = gcry_sexp_build(&platformKey, nullptr, "(public-key(ecc(curve %s)(q %b)))",
        curveName(curve), keyData.size(), keyData.data());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return nullptr;
    }

    return create(identifier, curve, CryptoKeyType::Public, platformKey.release(), extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPublic(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, bool extractable, CryptoKeyUsageBitmap usages)
{
    unsigned uncompressedFieldElementSize = uncompressedFieldElementSizeForCurve(curve);
    if (x.size() != uncompressedFieldElementSize || y.size() != uncompressedFieldElementSize)
        return nullptr;

    // Construct the Vector that represents the EC point in uncompressed format.
    Vector<uint8_t> q;
    q.reserveInitialCapacity(1 + 2 * uncompressedFieldElementSize);
    q.append(0x04);
    q.appendVector(x);
    q.appendVector(y);

    PAL::GCrypt::Handle<gcry_sexp_t> platformKey;
    gcry_error_t error = gcry_sexp_build(&platformKey, nullptr, "(public-key(ecc(curve %s)(q %b)))",
        curveName(curve), q.size(), q.data());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return nullptr;
    }

    return create(identifier, curve, CryptoKeyType::Public, platformKey.release(), extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportJWKPrivate(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& x, Vector<uint8_t>&& y, Vector<uint8_t>&& d, bool extractable, CryptoKeyUsageBitmap usages)
{
    unsigned uncompressedFieldElementSize = uncompressedFieldElementSizeForCurve(curve);
    if (x.size() != uncompressedFieldElementSize || y.size() != uncompressedFieldElementSize || d.size() != uncompressedFieldElementSize)
        return nullptr;

    // Construct the Vector that represents the EC point in uncompressed format.
    Vector<uint8_t> q;
    q.reserveInitialCapacity(1 + 2 * uncompressedFieldElementSize);
    q.append(0x04);
    q.appendVector(x);
    q.appendVector(y);

    PAL::GCrypt::Handle<gcry_sexp_t> platformKey;
    gcry_error_t error = gcry_sexp_build(&platformKey, nullptr, "(private-key(ecc(curve %s)(q %b)(d %b)))",
        curveName(curve), q.size(), q.data(), d.size(), d.data());
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return nullptr;
    }

    return create(identifier, curve, CryptoKeyType::Private, platformKey.release(), extractable, usages);
}

static bool supportedAlgorithmIdentifier(CryptoAlgorithmIdentifier keyIdentifier, const Vector<uint8_t>& identifier)
{
    static const std::array<uint8_t, 18> s_id_ecPublicKey { { "1.2.840.10045.2.1" } };
    static const std::array<uint8_t, 13> s_id_ecDH { { "1.3.132.1.12" } };

    auto size = identifier.size();
    auto* data = identifier.data();

    switch (keyIdentifier) {
    case CryptoAlgorithmIdentifier::ECDSA:
        // ECDSA only supports id-ecPublicKey algorithms for imported keys.
        if (size == s_id_ecPublicKey.size() && !std::memcmp(data, s_id_ecPublicKey.data(), size))
            return true;
        return false;
    case CryptoAlgorithmIdentifier::ECDH:
        // ECDH supports both id-ecPublicKey and ic-ecDH algorithms for imported keys.
        if (size == s_id_ecPublicKey.size() && !std::memcmp(data, s_id_ecPublicKey.data(), size))
            return true;
        if (size == s_id_ecDH.size() && !std::memcmp(data, s_id_ecDH.data(), size))
            return true;
        return false;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return false;
}

static std::optional<CryptoKeyEC::NamedCurve> curveForIdentifier(const Vector<uint8_t>& identifier)
{
    static const std::array<uint8_t, 20> s_secp256r1 { { "1.2.840.10045.3.1.7" } };
    static const std::array<uint8_t, 13> s_secp384r1 { { "1.3.132.0.34" } };
    static const std::array<uint8_t, 13> s_secp521r1 { { "1.3.132.0.35" } };

    auto size = identifier.size();
    auto* data = identifier.data();

    if (size == s_secp256r1.size() && !std::memcmp(data, s_secp256r1.data(), size))
        return CryptoKeyEC::NamedCurve::P256;
    if (size == s_secp384r1.size() && !std::memcmp(data, s_secp384r1.data(), size))
        return CryptoKeyEC::NamedCurve::P384;
    if (size == s_secp521r1.size() && !std::memcmp(data, s_secp521r1.data(), size))
        return std::nullopt; // Not yet supported.

    return std::nullopt;
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportSpki(CryptoAlgorithmIdentifier identifier, NamedCurve curve, Vector<uint8_t>&& keyData, bool extractable, CryptoKeyUsageBitmap usages)
{
    // Decode the `SubjectPublicKeyInfo` structure using the provided key data.
    PAL::TASN1::Structure spki;
    if (!PAL::TASN1::decodeStructure(&spki, "WebCrypto.SubjectPublicKeyInfo", keyData))
        return nullptr;

    // Validate `algorithm.algorithm`.
    {
        auto algorithm = PAL::TASN1::elementData(spki, "algorithm.algorithm");
        if (!algorithm)
            return nullptr;

        if (!supportedAlgorithmIdentifier(identifier, *algorithm))
            return nullptr;
    }

    // Validate `algorithm.parameters` and therein embedded `ECParameters`.
    {
        auto parameters = PAL::TASN1::elementData(spki, "algorithm.parameters");
        if (!parameters)
            return nullptr;

        // Decode the `ECParameters` structure using the `algorithm.parameters` data.
        PAL::TASN1::Structure ecParameters;
        if (!PAL::TASN1::decodeStructure(&ecParameters, "WebCrypto.ECParameters", *parameters))
            return nullptr;

        auto namedCurve = PAL::TASN1::elementData(ecParameters, "namedCurve");
        if (!namedCurve)
            return nullptr;

        auto parameterCurve = curveForIdentifier(*namedCurve);
        if (!parameterCurve || *parameterCurve != curve)
            return nullptr;
    }

    // Retrieve the `subjectPublicKey` data and embed it into the `public-key` s-expression.
    PAL::GCrypt::Handle<gcry_sexp_t> platformKey;
    {
        auto subjectPublicKey = PAL::TASN1::elementData(spki, "subjectPublicKey");
        if (!subjectPublicKey)
            return nullptr;

        // Bail if the `subjectPublicKey` data size doesn't match the size of an uncompressed point
        // for this curve, or if the first byte in the `subjectPublicKey` data isn't 0x04, as required
        // for an uncompressed EC point encoded in an octet string.
        if (subjectPublicKey->size() != uncompressedPointSizeForCurve(curve) || subjectPublicKey->at(0) != 0x04)
            return nullptr;

        // Convert X and Y coordinate data into MPIs.
        unsigned coordinateSize = uncompressedFieldElementSizeForCurve(curve);
        PAL::GCrypt::Handle<gcry_mpi_t> xMPI, yMPI;
        {
            gcry_error_t error = gcry_mpi_scan(&xMPI, GCRYMPI_FMT_USG, &subjectPublicKey->at(1), coordinateSize, nullptr);
            if (error != GPG_ERR_NO_ERROR) {
                PAL::GCrypt::logError(error);
                return nullptr;
            }

            error = gcry_mpi_scan(&yMPI, GCRYMPI_FMT_USG, &subjectPublicKey->at(1 + coordinateSize), coordinateSize, nullptr);
            if (error != GPG_ERR_NO_ERROR) {
                PAL::GCrypt::logError(error);
                return nullptr;
            }
        }

        // Construct an MPI point from the X and Y coordinates and using 1 as the Z coordinate.
        // This always allocates the gcry_mpi_point_t object.
        PAL::GCrypt::Handle<gcry_mpi_point_t> point(gcry_mpi_point_set(nullptr, xMPI, yMPI, GCRYMPI_CONST_ONE));

        // Create an EC context for the specified curve.
        PAL::GCrypt::Handle<gcry_ctx_t> context;
        gcry_error_t error = gcry_mpi_ec_new(&context, nullptr, curveName(curve));
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return nullptr;
        }

        // Bail if the constructed MPI point is not on the specified EC curve.
        if (!gcry_mpi_ec_curve_point(point, context))
            return nullptr;

        error = gcry_sexp_build(&platformKey, nullptr, "(public-key(ecc(curve %s)(q %b)))",
            curveName(curve), subjectPublicKey->size(), subjectPublicKey->data());
        if (error != GPG_ERR_NO_ERROR) {
            PAL::GCrypt::logError(error);
            return nullptr;
        }
    }

    // Finally create a new CryptoKeyEC object, transferring to it ownership of the `public-key` s-expression.
    return create(identifier, curve, CryptoKeyType::Public, platformKey.release(), extractable, usages);
}

RefPtr<CryptoKeyEC> CryptoKeyEC::platformImportPkcs8(CryptoAlgorithmIdentifier, NamedCurve, Vector<uint8_t>&&, bool, CryptoKeyUsageBitmap)
{
    notImplemented();

    return nullptr;
}

Vector<uint8_t> CryptoKeyEC::platformExportRaw() const
{
    PAL::GCrypt::Handle<gcry_ctx_t> context;
    gcry_error_t error = gcry_mpi_ec_new(&context, m_platformKey, nullptr);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return { };
    }

    PAL::GCrypt::Handle<gcry_mpi_t> qMPI(gcry_mpi_ec_get_mpi("q", context, 0));
    if (!qMPI)
        return { };

    auto q = mpiData(qMPI);
    if (!q || q->size() != uncompressedPointSizeForCurve(m_curve))
        return { };

    return WTFMove(q.value());
}

void CryptoKeyEC::platformAddFieldElements(JsonWebKey& jwk) const
{
    PAL::GCrypt::Handle<gcry_ctx_t> context;
    gcry_error_t error = gcry_mpi_ec_new(&context, m_platformKey, nullptr);
    if (error != GPG_ERR_NO_ERROR) {
        PAL::GCrypt::logError(error);
        return;
    }

    unsigned uncompressedFieldElementSize = uncompressedFieldElementSizeForCurve(m_curve);

    PAL::GCrypt::Handle<gcry_mpi_t> qMPI(gcry_mpi_ec_get_mpi("q", context, 0));
    if (qMPI) {
        auto q = mpiData(qMPI);
        if (q && q->size() == uncompressedPointSizeForCurve(m_curve)) {
            Vector<uint8_t> a;
            a.append(q->data() + 1, uncompressedFieldElementSize);
            jwk.x = base64URLEncode(a);

            Vector<uint8_t> b;
            b.append(q->data() + 1 + uncompressedFieldElementSize, uncompressedFieldElementSize);
            jwk.y = base64URLEncode(b);
        }
    }

    if (type() == Type::Private) {
        PAL::GCrypt::Handle<gcry_mpi_t> dMPI(gcry_mpi_ec_get_mpi("d", context, 0));
        if (dMPI) {
            auto d = mpiData(dMPI);
            if (d && d->size() == uncompressedFieldElementSize)
                jwk.d = base64URLEncode(*d);
        }
    }
}

Vector<uint8_t> CryptoKeyEC::platformExportSpki() const
{
    notImplemented();

    return { };
}

Vector<uint8_t> CryptoKeyEC::platformExportPkcs8() const
{
    notImplemented();

    return { };
}

} // namespace WebCore

#endif // ENABLE(SUBTLE_CRYPTO)
