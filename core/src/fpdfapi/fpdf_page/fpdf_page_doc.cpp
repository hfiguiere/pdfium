// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "../../../include/fpdfapi/fpdf_page.h"
#include "../../../include/fpdfapi/fpdf_module.h"
#include "../../../include/fdrm/fx_crypt.h"
#include "../fpdf_font/font_int.h"
#include "pageint.h"

namespace {

template <class KeyType, class ValueType>
KeyType PDF_DocPageData_FindValue(
    const CFX_MapPtrTemplate<KeyType, CPDF_CountedObject<ValueType>*>& map,
    ValueType findValue,
    CPDF_CountedObject<ValueType>*& findData)
{
    FX_POSITION pos = map.GetStartPosition();
    while (pos) {
        KeyType findKey;
        map.GetNextAssoc(pos, findKey, findData);
        if (findData->m_Obj == findValue) {
            return findKey;
        }
    }
    findData = nullptr;
    return (KeyType)nullptr;
}

template <class KeyType, class ValueType>
void PDF_DocPageData_Release(
    CFX_MapPtrTemplate<KeyType, CPDF_CountedObject<ValueType>*>& map,
    KeyType findKey,
    ValueType findValue,
    FX_BOOL bForce)
{
    if (!findKey && !findValue)
        return;

    CPDF_CountedObject<ValueType>* findData = nullptr;
    if (!findKey) {
        findKey = PDF_DocPageData_FindValue<KeyType, ValueType>(map, findValue, findData);
    } else if (!map.Lookup(findKey, findData)) {
        return;
    }
    if (!findData)
        return;

    if ((-- findData->m_nCount) == 0 || bForce) {
        delete findData->m_Obj;
        delete findData;
        map.RemoveKey(findKey);
    }
}

template <class KeyType, class ValueType>
void PDF_DocPageData_Release_Key(
    KeyType findKey,
    FX_BOOL bForce,
    std::map<KeyType, CPDF_CountedObject<ValueType>*>* map)
{
    if (!findKey)
        return;

    CPDF_CountedObject<ValueType>* findData = nullptr;
    auto it = map->find(findKey);
    if (it != map->end())
        findData = it->second;
    if (!findData)
        return;

    if ((--findData->m_nCount) == 0 || bForce) {
        delete findData->m_Obj;
        delete findData;
        map->erase(it);
    }
}

}  // namespace

class CPDF_PageModule : public CPDF_PageModuleDef
{
public:
    CPDF_PageModule() : m_StockGrayCS(PDFCS_DEVICEGRAY), m_StockRGBCS(PDFCS_DEVICERGB),
        m_StockCMYKCS(PDFCS_DEVICECMYK) {}
    virtual ~CPDF_PageModule() {}
    virtual FX_BOOL		Installed()
    {
        return TRUE;
    }
    virtual CPDF_DocPageData*	CreateDocData(CPDF_Document* pDoc)
    {
        return new CPDF_DocPageData(pDoc);
    }
    virtual void		ReleaseDoc(CPDF_Document* pDoc);
    virtual void		ClearDoc(CPDF_Document* pDoc);
    virtual CPDF_FontGlobals*	GetFontGlobals()
    {
        return &m_FontGlobals;
    }
    virtual void				ClearStockFont(CPDF_Document* pDoc)
    {
        m_FontGlobals.Clear(pDoc);
    }
    virtual CPDF_ColorSpace*	GetStockCS(int family);
    virtual void		NotifyCJKAvailable();
    CPDF_FontGlobals	m_FontGlobals;
    CPDF_DeviceCS		m_StockGrayCS;
    CPDF_DeviceCS		m_StockRGBCS;
    CPDF_DeviceCS		m_StockCMYKCS;
    CPDF_PatternCS		m_StockPatternCS;
};
CPDF_ColorSpace* CPDF_PageModule::GetStockCS(int family)
{
    if (family == PDFCS_DEVICEGRAY) {
        return &m_StockGrayCS;
    }
    if (family == PDFCS_DEVICERGB) {
        return &m_StockRGBCS;
    }
    if (family == PDFCS_DEVICECMYK) {
        return &m_StockCMYKCS;
    }
    if (family == PDFCS_PATTERN) {
        return &m_StockPatternCS;
    }
    return NULL;
}
void CPDF_ModuleMgr::InitPageModule()
{
    delete m_pPageModule;
    m_pPageModule = new CPDF_PageModule;
}
void CPDF_PageModule::ReleaseDoc(CPDF_Document* pDoc)
{
    delete pDoc->GetPageData();
}
void CPDF_PageModule::ClearDoc(CPDF_Document* pDoc)
{
    pDoc->GetPageData()->Clear(FALSE);
}
void CPDF_PageModule::NotifyCJKAvailable()
{
    m_FontGlobals.m_CMapManager.ReloadAll();
}
CPDF_Font* CPDF_Document::LoadFont(CPDF_Dictionary* pFontDict)
{
    if (!pFontDict) {
        return NULL;
    }
    return GetValidatePageData()->GetFont(pFontDict, FALSE);
}
CPDF_Font* CPDF_Document::FindFont(CPDF_Dictionary* pFontDict)
{
    if (!pFontDict) {
        return NULL;
    }
    return GetValidatePageData()->GetFont(pFontDict, TRUE);
}
CPDF_StreamAcc* CPDF_Document::LoadFontFile(CPDF_Stream* pStream)
{
    if (pStream == NULL) {
        return NULL;
    }
    return GetValidatePageData()->GetFontFileStreamAcc(pStream);
}
CPDF_ColorSpace* _CSFromName(const CFX_ByteString& name);
CPDF_ColorSpace* CPDF_Document::LoadColorSpace(CPDF_Object* pCSObj, CPDF_Dictionary* pResources)
{
    return GetValidatePageData()->GetColorSpace(pCSObj, pResources);
}
CPDF_Pattern* CPDF_Document::LoadPattern(CPDF_Object* pPatternObj, FX_BOOL bShading, const CFX_AffineMatrix* matrix)
{
    return GetValidatePageData()->GetPattern(pPatternObj, bShading, matrix);
}
CPDF_IccProfile* CPDF_Document::LoadIccProfile(CPDF_Stream* pStream)
{
    return GetValidatePageData()->GetIccProfile(pStream);
}
CPDF_Image* CPDF_Document::LoadImageF(CPDF_Object* pObj)
{
    if (!pObj) {
        return NULL;
    }
    FXSYS_assert(pObj->GetObjNum());
    return GetValidatePageData()->GetImage(pObj);
}
void CPDF_Document::RemoveColorSpaceFromPageData(CPDF_Object* pCSObj)
{
    if (!pCSObj) {
        return;
    }
    GetPageData()->ReleaseColorSpace(pCSObj);
}
CPDF_DocPageData::CPDF_DocPageData(CPDF_Document *pPDFDoc)
    : m_pPDFDoc(pPDFDoc),
      m_IccProfileMap(),
      m_FontFileMap(),
      m_bForceClear(FALSE)
{
    m_IccProfileMap.InitHashTable(16);
    m_FontFileMap.InitHashTable(32);
}

CPDF_DocPageData::~CPDF_DocPageData()
{
    Clear(FALSE);
    Clear(TRUE);

    for (auto& it : m_PatternMap)
        delete it.second;
    m_PatternMap.clear();

    for (auto& it : m_FontMap)
        delete it.second;
    m_FontMap.clear();

    for (auto& it : m_ColorSpaceMap)
        delete it.second;
    m_ColorSpaceMap.clear();
}

void CPDF_DocPageData::Clear(FX_BOOL bForceRelease)
{
    m_bForceClear = bForceRelease;

    for (auto& it : m_PatternMap) {
        CPDF_CountedPattern* ptData = it.second;
        if (!ptData->m_Obj)
            continue;

        if (bForceRelease || ptData->m_nCount < 2) {
            ptData->m_Obj->SetForceClear(bForceRelease);
            delete ptData->m_Obj;
            ptData->m_Obj = nullptr;
        }
    }

    for (auto& it : m_FontMap) {
        CPDF_CountedFont* fontData = it.second;
        if (!fontData->m_Obj)
            continue;

        if (bForceRelease || fontData->m_nCount < 2) {
            delete fontData->m_Obj;
            fontData->m_Obj = nullptr;
        }
    }

    for (auto& it : m_ColorSpaceMap) {
        CPDF_CountedColorSpace* csData = it.second;
        if (!csData->m_Obj)
            continue;

        if (bForceRelease || csData->m_nCount < 2) {
            csData->m_Obj->ReleaseCS();
            csData->m_Obj = nullptr;
        }
    }

    FX_POSITION pos = m_IccProfileMap.GetStartPosition();
    while (pos) {
        CPDF_Stream* ipKey;
        CPDF_CountedObject<CPDF_IccProfile*>* ipData;
        m_IccProfileMap.GetNextAssoc(pos, ipKey, ipData);
        if (!ipData->m_Obj) {
            continue;
        }
        if (bForceRelease || ipData->m_nCount < 2) {
            FX_POSITION pos2 = m_HashProfileMap.GetStartPosition();
            while (pos2) {
                CFX_ByteString bsKey;
                CPDF_Stream* pFindStream = NULL;
                m_HashProfileMap.GetNextAssoc(pos2, bsKey, (void*&)pFindStream);
                if (ipKey == pFindStream) {
                    m_HashProfileMap.RemoveKey(bsKey);
                    break;
                }
            }
            delete ipData->m_Obj;
            delete ipData;
            m_IccProfileMap.RemoveKey(ipKey);
        }
    }
    pos = m_FontFileMap.GetStartPosition();
    while (pos) {
        CPDF_Stream* ftKey;
        CPDF_CountedObject<CPDF_StreamAcc*>* ftData;
        m_FontFileMap.GetNextAssoc(pos, ftKey, ftData);
        if (!ftData->m_Obj) {
            continue;
        }
        if (bForceRelease || ftData->m_nCount < 2) {
            delete ftData->m_Obj;
            delete ftData;
            m_FontFileMap.RemoveKey(ftKey);
        }
    }

    for (auto it = m_ImageMap.begin(); it != m_ImageMap.end();) {
        auto curr_it = it++;
        CPDF_CountedImage* imageData = curr_it->second;
        if (!imageData->m_Obj)
            continue;

        if (bForceRelease || imageData->m_nCount < 2) {
            delete imageData->m_Obj;
            delete imageData;
            m_ImageMap.erase(curr_it);
        }
    }
}

CPDF_Font* CPDF_DocPageData::GetFont(CPDF_Dictionary* pFontDict, FX_BOOL findOnly)
{
    if (!pFontDict) {
        return NULL;
    }
    if (findOnly) {
        auto it = m_FontMap.find(pFontDict);
        if (it != m_FontMap.end()) {
            CPDF_CountedFont* fontData = it->second;
            if (!fontData->m_Obj)
                return nullptr;

            fontData->m_nCount++;
            return fontData->m_Obj;
        }
        return nullptr;
    }

    CPDF_CountedFont* fontData = nullptr;
    auto it = m_FontMap.find(pFontDict);
    if (it != m_FontMap.end()) {
        fontData = it->second;
        if (fontData->m_Obj) {
            fontData->m_nCount++;
            return fontData->m_Obj;
        }
    }

    FX_BOOL bNew = FALSE;
    if (!fontData) {
        fontData = new CPDF_CountedFont;
        bNew = TRUE;
    }
    CPDF_Font* pFont = CPDF_Font::CreateFontF(m_pPDFDoc, pFontDict);
    if (!pFont) {
        if (bNew)
            delete fontData;
        return nullptr;
    }
    fontData->m_nCount = 2;
    fontData->m_Obj = pFont;
    if (bNew)
        m_FontMap[pFontDict] = fontData;
    return pFont;
}

CPDF_Font* CPDF_DocPageData::GetStandardFont(const CFX_ByteStringC& fontName, CPDF_FontEncoding* pEncoding)
{
    if (fontName.IsEmpty())
        return nullptr;

    for (auto& it : m_FontMap) {
        CPDF_CountedFont* fontData = it.second;
        CPDF_Font* pFont = fontData->m_Obj;
        if (!pFont)
            continue;
        if (pFont->GetBaseFont() != fontName)
            continue;
        if (pFont->IsEmbedded())
            continue;
        if (pFont->GetFontType() != PDFFONT_TYPE1)
            continue;
        if (pFont->GetFontDict()->KeyExist(FX_BSTRC("Widths")))
            continue;

        CPDF_Type1Font* pT1Font = pFont->GetType1Font();
        if (pEncoding && !pT1Font->GetEncoding()->IsIdentical(pEncoding))
            continue;

        fontData->m_nCount++;
        return pFont;
    }

    CPDF_Dictionary* pDict = new CPDF_Dictionary;
    pDict->SetAtName(FX_BSTRC("Type"), FX_BSTRC("Font"));
    pDict->SetAtName(FX_BSTRC("Subtype"), FX_BSTRC("Type1"));
    pDict->SetAtName(FX_BSTRC("BaseFont"), fontName);
    if (pEncoding) {
        pDict->SetAt(FX_BSTRC("Encoding"), pEncoding->Realize());
    }
    m_pPDFDoc->AddIndirectObject(pDict);
    CPDF_CountedFont* fontData = new CPDF_CountedFont;
    CPDF_Font* pFont = CPDF_Font::CreateFontF(m_pPDFDoc, pDict);
    if (!pFont) {
        delete fontData;
        return nullptr;
    }
    fontData->m_nCount = 2;
    fontData->m_Obj = pFont;
    m_FontMap[pDict] = fontData;
    return pFont;
}

void CPDF_DocPageData::ReleaseFont(CPDF_Dictionary* pFontDict)
{
    if (!pFontDict)
        return;

    auto it = m_FontMap.find(pFontDict);
    if (it == m_FontMap.end())
        return;

    CPDF_CountedFont* fontData = it->second;
    if (fontData->m_Obj && --fontData->m_nCount == 0) {
        delete fontData->m_Obj;
        fontData->m_Obj = nullptr;
    }
}

CPDF_ColorSpace* CPDF_DocPageData::GetColorSpace(CPDF_Object* pCSObj, CPDF_Dictionary* pResources)
{
    if (!pCSObj) {
        return NULL;
    }
    if (pCSObj->GetType() == PDFOBJ_NAME) {
        CFX_ByteString name = pCSObj->GetConstString();
        CPDF_ColorSpace* pCS = _CSFromName(name);
        if (!pCS && pResources) {
            CPDF_Dictionary* pList = pResources->GetDict(FX_BSTRC("ColorSpace"));
            if (pList) {
                pCSObj = pList->GetElementValue(name);
                return GetColorSpace(pCSObj, NULL);
            }
        }
        if (pCS == NULL || pResources == NULL) {
            return pCS;
        }
        CPDF_Dictionary* pColorSpaces = pResources->GetDict(FX_BSTRC("ColorSpace"));
        if (pColorSpaces == NULL) {
            return pCS;
        }
        CPDF_Object* pDefaultCS = NULL;
        switch (pCS->GetFamily()) {
            case PDFCS_DEVICERGB:
                pDefaultCS = pColorSpaces->GetElementValue(FX_BSTRC("DefaultRGB"));
                break;
            case PDFCS_DEVICEGRAY:
                pDefaultCS = pColorSpaces->GetElementValue(FX_BSTRC("DefaultGray"));
                break;
            case PDFCS_DEVICECMYK:
                pDefaultCS = pColorSpaces->GetElementValue(FX_BSTRC("DefaultCMYK"));
                break;
        }
        if (pDefaultCS == NULL) {
            return pCS;
        }
        return GetColorSpace(pDefaultCS, NULL);
    }

    if (pCSObj->GetType() != PDFOBJ_ARRAY)
        return nullptr;
    CPDF_Array* pArray = (CPDF_Array*)pCSObj;
    if (pArray->GetCount() == 0)
        return nullptr;
    if (pArray->GetCount() == 1)
        return GetColorSpace(pArray->GetElementValue(0), pResources);

    CPDF_CountedColorSpace* csData = nullptr;
    auto it = m_ColorSpaceMap.find(pCSObj);
    if (it != m_ColorSpaceMap.end()) {
        csData = it->second;
        if (csData->m_Obj) {
            csData->m_nCount++;
            return csData->m_Obj;
        }
    }

    CPDF_ColorSpace* pCS = CPDF_ColorSpace::Load(m_pPDFDoc, pArray);
    if (!pCS)
        return nullptr;

    if (!csData) {
        csData = new CPDF_CountedColorSpace;
        m_ColorSpaceMap[pCSObj] = csData;
    }
    csData->m_nCount = 2;
    csData->m_Obj = pCS;
    return pCS;
}

CPDF_ColorSpace* CPDF_DocPageData::GetCopiedColorSpace(CPDF_Object* pCSObj)
{
    if (!pCSObj)
        return nullptr;

    auto it = m_ColorSpaceMap.find(pCSObj);
    if (it == m_ColorSpaceMap.end())
        return nullptr;

    CPDF_CountedColorSpace* csData = it->second;
    if (!csData->m_Obj)
        return nullptr;

    csData->m_nCount++;
    return csData->m_Obj;
}

void CPDF_DocPageData::ReleaseColorSpace(CPDF_Object* pColorSpace)
{
    if (!pColorSpace)
        return;

    auto it = m_ColorSpaceMap.find(pColorSpace);
    if (it == m_ColorSpaceMap.end())
        return;

    CPDF_CountedColorSpace* csData = it->second;
    if (csData->m_Obj && --csData->m_nCount == 0) {
        csData->m_Obj->ReleaseCS();
        csData->m_Obj = nullptr;
    }
}

CPDF_Pattern* CPDF_DocPageData::GetPattern(CPDF_Object* pPatternObj, FX_BOOL bShading, const CFX_AffineMatrix* matrix)
{
    if (!pPatternObj)
        return nullptr;

    CPDF_CountedPattern* ptData = nullptr;
    auto it = m_PatternMap.find(pPatternObj);
    if (it != m_PatternMap.end()) {
        ptData = it->second;
        if (ptData->m_Obj) {
            ptData->m_nCount++;
            return ptData->m_Obj;
        }
    }
    CPDF_Pattern* pPattern = nullptr;
    if (bShading) {
        pPattern = new CPDF_ShadingPattern(m_pPDFDoc, pPatternObj, bShading, matrix);
    } else {
        CPDF_Dictionary* pDict = pPatternObj ? pPatternObj->GetDict() : nullptr;
        if (pDict) {
            int type = pDict->GetInteger(FX_BSTRC("PatternType"));
            if (type == 1) {
                pPattern = new CPDF_TilingPattern(m_pPDFDoc, pPatternObj, matrix);
            } else if (type == 2) {
                pPattern = new CPDF_ShadingPattern(m_pPDFDoc, pPatternObj, FALSE, matrix);
            }
        }
    }
    if (!pPattern)
        return nullptr;

    if (!ptData) {
        ptData = new CPDF_CountedPattern;
        m_PatternMap[pPatternObj] = ptData;
    }
    ptData->m_nCount = 2;
    ptData->m_Obj = pPattern;
    return pPattern;
}

void CPDF_DocPageData::ReleasePattern(CPDF_Object* pPatternObj)
{
    if (!pPatternObj)
        return;

    auto it = m_PatternMap.find(pPatternObj);
    if (it == m_PatternMap.end())
        return;

    CPDF_CountedPattern* ptData = it->second;
    if (ptData->m_Obj && --ptData->m_nCount == 0) {
        delete ptData->m_Obj;
        ptData->m_Obj = nullptr;
    }
}

CPDF_Image* CPDF_DocPageData::GetImage(CPDF_Object* pImageStream)
{
    if (!pImageStream)
        return nullptr;

    const FX_DWORD dwImageObjNum = pImageStream->GetObjNum();
    auto it = m_ImageMap.find(dwImageObjNum);
    if (it != m_ImageMap.end()) {
        CPDF_CountedImage* imageData = it->second;
        imageData->m_nCount++;
        return imageData->m_Obj;
    }
    CPDF_CountedImage* imageData = new CPDF_CountedImage;
    CPDF_Image* pImage = new CPDF_Image(m_pPDFDoc);
    pImage->LoadImageF((CPDF_Stream*)pImageStream, FALSE);
    imageData->m_nCount = 2;
    imageData->m_Obj = pImage;
    m_ImageMap[dwImageObjNum] = imageData;
    return pImage;
}

void CPDF_DocPageData::ReleaseImage(CPDF_Object* pImageStream)
{
    if (!pImageStream)
        return;

    PDF_DocPageData_Release_Key<FX_DWORD, CPDF_Image*>(
        pImageStream->GetObjNum(), FALSE, &m_ImageMap);
}

CPDF_IccProfile* CPDF_DocPageData::GetIccProfile(CPDF_Stream* pIccProfileStream)
{
    if (!pIccProfileStream) {
        return NULL;
    }
    CPDF_CountedObject<CPDF_IccProfile*>* ipData = NULL;
    if (m_IccProfileMap.Lookup(pIccProfileStream, ipData)) {
        ipData->m_nCount++;
        return ipData->m_Obj;
    }
    CPDF_StreamAcc stream;
    stream.LoadAllData(pIccProfileStream, FALSE);
    uint8_t digest[20];
    CPDF_Stream* pCopiedStream = NULL;
    CRYPT_SHA1Generate(stream.GetData(), stream.GetSize(), digest);
    if (m_HashProfileMap.Lookup(CFX_ByteStringC(digest, 20), (void*&)pCopiedStream)) {
        m_IccProfileMap.Lookup(pCopiedStream, ipData);
        ipData->m_nCount++;
        return ipData->m_Obj;
    }
    CPDF_IccProfile* pProfile = new CPDF_IccProfile(stream.GetData(), stream.GetSize());
    ipData = new CPDF_CountedObject<CPDF_IccProfile*>;
    ipData->m_nCount = 2;
    ipData->m_Obj = pProfile;
    m_IccProfileMap.SetAt(pIccProfileStream, ipData);
    m_HashProfileMap.SetAt(CFX_ByteStringC(digest, 20), pIccProfileStream);
    return pProfile;
}

void CPDF_DocPageData::ReleaseIccProfile(CPDF_IccProfile* pIccProfile)
{
    ASSERT(pIccProfile);
    PDF_DocPageData_Release<CPDF_Stream*, CPDF_IccProfile*>(
        m_IccProfileMap, nullptr, pIccProfile, FALSE);
}

CPDF_StreamAcc* CPDF_DocPageData::GetFontFileStreamAcc(CPDF_Stream* pFontStream)
{
    if (!pFontStream) {
        return NULL;
    }
    CPDF_CountedObject<CPDF_StreamAcc*>* ftData;
    if (m_FontFileMap.Lookup(pFontStream, ftData)) {
        ftData->m_nCount ++;
        return ftData->m_Obj;
    }
    ftData = new CPDF_CountedObject<CPDF_StreamAcc*>;
    CPDF_StreamAcc* pFontFile = new CPDF_StreamAcc;
    CPDF_Dictionary* pFontDict = pFontStream->GetDict();
    int32_t org_size = pFontDict->GetInteger(FX_BSTRC("Length1")) + pFontDict->GetInteger(FX_BSTRC("Length2")) + pFontDict->GetInteger(FX_BSTRC("Length3"));
    if (org_size < 0) {
        org_size = 0;
    }
    pFontFile->LoadAllData(pFontStream, FALSE, org_size);
    ftData->m_nCount = 2;
    ftData->m_Obj = pFontFile;
    m_FontFileMap.SetAt(pFontStream, ftData);
    return pFontFile;
}
void CPDF_DocPageData::ReleaseFontFileStreamAcc(CPDF_Stream* pFontStream, FX_BOOL bForce)
{
    if (!pFontStream) {
        return;
    }
    PDF_DocPageData_Release<CPDF_Stream*, CPDF_StreamAcc*>(m_FontFileMap, pFontStream, NULL, bForce);
}

CPDF_CountedColorSpace* CPDF_DocPageData::FindColorSpacePtr(CPDF_Object* pCSObj) const
{
    if (!pCSObj)
        return nullptr;

    auto it = m_ColorSpaceMap.find(pCSObj);
    return it != m_ColorSpaceMap.end() ? it->second : nullptr;
}

CPDF_CountedPattern* CPDF_DocPageData::FindPatternPtr(CPDF_Object* pPatternObj) const
{
    if (!pPatternObj)
        return nullptr;

    auto it = m_PatternMap.find(pPatternObj);
    return it != m_PatternMap.end() ? it->second : nullptr;
}
