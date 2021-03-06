/*
 *  Copyright (C) 2017-2018 Team Kodi
 *  This file is part of Kodi - https://kodi.tv
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *  See LICENSES/README.md for more information.
 */

#pragma once

#include "../AddonBase.h"
#include "../StreamCrypto.h"
#include "../StreamCodec.h"

#ifdef BUILD_KODI_ADDON
#include "../DemuxPacket.h"
#else
#include "cores/VideoPlayer/Interface/Addon/DemuxPacket.h"
#endif

namespace kodi { namespace addon { class CInstanceVideoCodec; } }

extern "C"
{
  enum VIDEOCODEC_FORMAT
  {
    UnknownVideoFormat = 0,
    VideoFormatYV12,
    VideoFormatI420,
    MaxVideoFormats
  };


  struct VIDEOCODEC_INITDATA
  {
    enum Codec {
      CodecUnknown = 0,
      CodecVp8,
      CodecH264,
      CodecVp9
    } codec;

    STREAMCODEC_PROFILE codecProfile;

    //UnknownVideoFormat is terminator
    VIDEOCODEC_FORMAT *videoFormats;

    uint32_t width, height;

    const uint8_t *extraData;
    unsigned int extraDataSize;

    CRYPTO_INFO cryptoInfo;
  };

  struct VIDEOCODEC_PICTURE
  {
    enum VideoPlane {
      YPlane = 0,
      UPlane,
      VPlane,
      MaxPlanes = 3,
    };

    enum Flags : uint32_t {
      FLAG_DROP,
      FLAG_DRAIN
    };

    VIDEOCODEC_FORMAT videoFormat;
    uint32_t flags;

    uint32_t width, height;

    uint8_t *decodedData;
    size_t decodedDataSize;

    uint32_t planeOffsets[VideoPlane::MaxPlanes];
    uint32_t stride[VideoPlane::MaxPlanes];

    int64_t pts;

    KODI_HANDLE buffer; //< will be passed in release_frame_buffer
  };

  enum VIDEOCODEC_RETVAL
  {
    VC_NONE = 0,        //< noop
    VC_ERROR,           //< an error occured, no other messages will be returned
    VC_BUFFER,          //< the decoder needs more data
    VC_PICTURE,         //< the decoder got a picture
    VC_EOF,             //< the decoder signals EOF
  };

  // this are properties given to the addon on create
  // at this time we have no parameters for the addon
  typedef struct AddonProps_VideoCodec
  {
    int dummy;
  } AddonProps_VideoCodec;

  struct AddonInstance_VideoCodec;
  typedef struct KodiToAddonFuncTable_VideoCodec
  {
    kodi::addon::CInstanceVideoCodec* addonInstance;

    //! \brief Opens a codec
    bool (__cdecl* open) (const AddonInstance_VideoCodec* instance, VIDEOCODEC_INITDATA *initData);

    //! \brief Reconfigures a codec
    bool (__cdecl* reconfigure) (const AddonInstance_VideoCodec* instance, VIDEOCODEC_INITDATA *initData);

    //! \brief Feed codec if requested from GetPicture() (return VC_BUFFER)
    bool (__cdecl* add_data) (const AddonInstance_VideoCodec* instance, const DemuxPacket *packet);

    //! \brief Get a decoded picture / request new data
    VIDEOCODEC_RETVAL (__cdecl* get_picture) (const AddonInstance_VideoCodec* instance, VIDEOCODEC_PICTURE *picture);

    //! \brief Get the name of this video decoder
    const char *(__cdecl* get_name) (const AddonInstance_VideoCodec* instance);

    //! \brief Reset the codec
    void (__cdecl* reset)(const AddonInstance_VideoCodec* instance);
  } KodiToAddonFuncTable_VideoCodec;

  typedef struct AddonToKodiFuncTable_VideoCodec
  {
    KODI_HANDLE kodiInstance;
    bool(*get_frame_buffer)(void* kodiInstance, VIDEOCODEC_PICTURE *picture);
    void(*release_frame_buffer)(void* kodiInstance, void *buffer);
  } AddonToKodiFuncTable_VideoCodec;

  typedef struct AddonInstance_VideoCodec
  {
    AddonProps_VideoCodec props;
    AddonToKodiFuncTable_VideoCodec toKodi;
    KodiToAddonFuncTable_VideoCodec toAddon;
  } AddonInstance_VideoCodec;
}

namespace kodi
{
  namespace addon
  {

    class ATTRIBUTE_HIDDEN CInstanceVideoCodec : public IAddonInstance
    {
    public:
      explicit CInstanceVideoCodec(KODI_HANDLE instance, const std::string& kodiVersion = "")
        : IAddonInstance(ADDON_INSTANCE_VIDEOCODEC,
                         !kodiVersion.empty() ? kodiVersion
                                              : GetKodiTypeVersion(ADDON_INSTANCE_VIDEOCODEC))
      {
        if (CAddonBase::m_interface->globalSingleInstance != nullptr)
          throw std::logic_error("kodi::addon::CInstanceVideoCodec: Creation of multiple together with single instance way is not allowed!");

        SetAddonStruct(instance);
      }

      ~CInstanceVideoCodec() override = default;

      //! \copydoc CInstanceVideoCodec::Open
      virtual bool Open(VIDEOCODEC_INITDATA &initData) { return false; };

      //! \copydoc CInstanceVideoCodec::Reconfigure
      virtual bool Reconfigure(VIDEOCODEC_INITDATA &initData) { return false; };

      //! \copydoc CInstanceVideoCodec::AddData
      virtual bool AddData(const DemuxPacket &packet) { return false; };

      //! \copydoc CInstanceVideoCodec::GetPicture
      virtual VIDEOCODEC_RETVAL GetPicture(VIDEOCODEC_PICTURE &picture) { return VC_ERROR; };

      //! \copydoc CInstanceVideoCodec::GetName
      virtual const char *GetName() { return nullptr; };

      //! \copydoc CInstanceVideoCodec::Reset
      virtual void Reset() {};

      /*!
      * @brief AddonToKodi interface
      */

      //! \copydoc CInstanceVideoCodec::GetFrameBuffer
      bool GetFrameBuffer(VIDEOCODEC_PICTURE &picture)
      {
        return m_instanceData->toKodi.get_frame_buffer(m_instanceData->toKodi.kodiInstance, &picture);
      }

      //! \copydoc CInstanceVideoCodec::ReleaseFrameBuffer
      void ReleaseFrameBuffer(void *buffer)
      {
        return m_instanceData->toKodi.release_frame_buffer(m_instanceData->toKodi.kodiInstance, buffer);
      }

    private:
      void SetAddonStruct(KODI_HANDLE instance)
      {
        if (instance == nullptr)
          throw std::logic_error("kodi::addon::CInstanceVideoCodec: Creation with empty addon structure not allowed, table must be given from Kodi!");

        m_instanceData = static_cast<AddonInstance_VideoCodec*>(instance);

        m_instanceData->toAddon.addonInstance = this;
        m_instanceData->toAddon.open = ADDON_Open;
        m_instanceData->toAddon.reconfigure = ADDON_Reconfigure;
        m_instanceData->toAddon.add_data = ADDON_AddData;
        m_instanceData->toAddon.get_picture = ADDON_GetPicture;
        m_instanceData->toAddon.get_name = ADDON_GetName;
        m_instanceData->toAddon.reset = ADDON_Reset;
      }

      inline static bool ADDON_Open(const AddonInstance_VideoCodec* instance, VIDEOCODEC_INITDATA *initData)
      {
        return instance->toAddon.addonInstance->Open(*initData);
      }

      inline static bool ADDON_Reconfigure(const AddonInstance_VideoCodec* instance, VIDEOCODEC_INITDATA *initData)
      {
        return instance->toAddon.addonInstance->Reconfigure(*initData);
      }

      inline static bool ADDON_AddData(const AddonInstance_VideoCodec* instance, const DemuxPacket *packet)
      {
        return instance->toAddon.addonInstance->AddData(*packet);
      }

      inline static VIDEOCODEC_RETVAL ADDON_GetPicture(const AddonInstance_VideoCodec* instance, VIDEOCODEC_PICTURE *picture)
      {
        return instance->toAddon.addonInstance->GetPicture(*picture);
      }

      inline static const char *ADDON_GetName(const AddonInstance_VideoCodec* instance)
      {
        return instance->toAddon.addonInstance->GetName();
      }

      inline static void ADDON_Reset(const AddonInstance_VideoCodec* instance)
      {
        return instance->toAddon.addonInstance->Reset();
      }

      AddonInstance_VideoCodec* m_instanceData;
    };
  } // namespace addon
} // namespace kodi
