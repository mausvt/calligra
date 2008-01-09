/*
 *  Copyright (c) 2003 Patrick Julien  <freak@codepimps.org>
 *  Copyright (c) 2004 Cyrille Berger <cberger@cberger.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/

#include "KoColorSpaceRegistry.h"

#include <QHash>

#include <QStringList>
#include <QDir>

#include <kcomponentdata.h>
#include <kstandarddirs.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kservice.h>
#include <kservicetypetrader.h>

#include <lcms.h>

#include "KoPluginLoader.h"
#include "KoColorSpace.h"
#include "KoColorConversionCache.h"
#include "KoColorConversionSystem.h"
#include "KoBasicHistogramProducers.h"

#include "colorprofiles/KoLcmsColorProfileContainer.h"
#include "colorspaces/KoAlphaColorSpace.h"
#include "colorspaces/KoLabColorSpace.h"
#include "colorspaces/KoRgbU16ColorSpace.h"
#include "colorspaces/KoRgbU8ColorSpace.h"

struct KoColorSpaceRegistry::Private {
    QHash<QString, KoColorProfile * > profileMap;
    QHash<QString, const KoColorSpace * > csMap;
    typedef QList<KisPaintDeviceAction *> PaintActionList;
    QHash<QString, PaintActionList> paintDevActionMap;
    const KoColorSpace *alphaCs;
    KoColorConversionSystem *colorConversionSystem;
    KoColorConversionCache* colorConversionCache;
    static KoColorSpaceRegistry *singleton;
    const KoColorSpace *rgbU8sRGB;
    const KoColorSpace *lab16sLAB;
};

KoColorSpaceRegistry *KoColorSpaceRegistry::Private::singleton = 0;

KoColorSpaceRegistry* KoColorSpaceRegistry::instance()
{
    if(KoColorSpaceRegistry::Private::singleton == 0)
    {
        KoColorSpaceRegistry::Private::singleton = new KoColorSpaceRegistry();
        KoColorSpaceRegistry::Private::singleton->init();
    }
    return KoColorSpaceRegistry::Private::singleton;
}


void KoColorSpaceRegistry::init()
{
    d->rgbU8sRGB = 0;
    d->lab16sLAB = 0;
    d->colorConversionSystem = new KoColorConversionSystem;
    d->colorConversionCache = new KoColorConversionCache;
    // prepare a list of the profiles
    KGlobal::mainComponent().dirs()->addResourceType("icc_profiles", 0, "share/color/icc/");

    QStringList profileFilenames;
    profileFilenames += KGlobal::mainComponent().dirs()->findAllResources("icc_profiles", "*.icm",  KStandardDirs::Recursive);
    profileFilenames += KGlobal::mainComponent().dirs()->findAllResources("icc_profiles", "*.ICM",  KStandardDirs::Recursive);
    profileFilenames += KGlobal::mainComponent().dirs()->findAllResources("icc_profiles", "*.ICC",  KStandardDirs::Recursive);
    profileFilenames += KGlobal::mainComponent().dirs()->findAllResources("icc_profiles", "*.icc",  KStandardDirs::Recursive);
    // Set lcms to return NUll/false etc from failing calls, rather than aborting the app.
    cmsErrorAction(LCMS_ERROR_SHOW);

    // Load the profiles
    if (!profileFilenames.empty()) {
        KoColorProfile * profile = 0;
        for ( QStringList::Iterator it = profileFilenames.begin(); it != profileFilenames.end(); ++it ) {
            profile = new KoIccColorProfile(*it);
            Q_CHECK_PTR(profile);

            profile->load();
            if (profile->valid()) {
                kDebug(DBG_PIGMENT) << "Valid profile : " << profile->name();
                d->profileMap[profile->name()] = profile;
            } else {
                kDebug(DBG_PIGMENT) << "Invalid profile : " << profile->name();
            }
        }
    }

    KoColorProfile *labProfile = KoLcmsColorProfileContainer::createFromLcmsProfile(cmsCreateLabProfile(NULL));
    addProfile(labProfile);
    add(new KoLabColorSpaceFactory());
    KoHistogramProducerFactoryRegistry::instance()->add(
                new KoBasicHistogramProducerFactory<KoBasicU16HistogramProducer>
                (KoID("LABAHISTO", i18n("L*a*b* Histogram")), lab16()));
    KoColorProfile *rgbProfile = KoLcmsColorProfileContainer::createFromLcmsProfile(cmsCreate_sRGBProfile());
    addProfile(rgbProfile);
    add(new KoRgbU16ColorSpaceFactory());

    add(new KoRgbU8ColorSpaceFactory());
    KoHistogramProducerFactoryRegistry::instance()->add(
                new KoBasicHistogramProducerFactory<KoBasicU8HistogramProducer>
                (KoID("RGB8HISTO", i18n("RGB8 Histogram")), rgb8()) );



    // Create the default profile for grayscale, probably not the best place to but that, but still better than in a grayscale plugin
    // .22 gamma grayscale or something like that. Taken from the lcms tutorial...
    LPGAMMATABLE Gamma = cmsBuildGamma(256, 2.2);
    cmsHPROFILE hProfile = cmsCreateGrayProfile(cmsD50_xyY(), Gamma);
    cmsFreeGamma(Gamma);
    KoColorProfile *defProfile = KoLcmsColorProfileContainer::createFromLcmsProfile(hProfile);
    kDebug(DBG_PIGMENT) << "Gray " << defProfile->name();
    addProfile(defProfile);

    // Create the built-in colorspaces
    d->alphaCs = new KoAlphaColorSpace();

    KoPluginLoader::PluginsConfig config;
    config.whiteList = "ColorSpacePlugins";
    config.blacklist = "ColorSpacePluginsDisabled";
    config.group = "koffice";
    KoPluginLoader::instance()->load("KOffice/ColorSpace","[X-Pigment-MinVersion] <= 0", config);

    KoPluginLoader::PluginsConfig configExtensions;
    configExtensions.whiteList = "ColorSpaceExtensionsPlugins";
    configExtensions.blacklist = "ColorSpaceExtensionsPluginsDisabled";
    configExtensions.group = "koffice";
    KoPluginLoader::instance()->load("KOffice/ColorSpaceExtension","[X-Pigment-MinVersion] <= 0", configExtensions);
}

KoColorSpaceRegistry::KoColorSpaceRegistry() : d(new Private())
{
    d->colorConversionSystem = 0;
    d->colorConversionCache = 0;
}

KoColorSpaceRegistry::~KoColorSpaceRegistry()
{
    delete d->colorConversionSystem;
    delete d->colorConversionCache;
    delete d;
}

void KoColorSpaceRegistry::add(KoColorSpaceFactory* item)
{
    KoGenericRegistry<KoColorSpaceFactory *>::add(item);
    d->colorConversionSystem->insertColorSpace(item);
}

void KoColorSpaceRegistry::add(const QString &id, KoColorSpaceFactory* item)
{
    KoGenericRegistry<KoColorSpaceFactory *>::add(id, item);
    d->colorConversionSystem->insertColorSpace(item);
}

const KoColorProfile *  KoColorSpaceRegistry::profileByName(const QString & name) const
{
    if (d->profileMap.find(name) == d->profileMap.end()) {
        return 0;
    }

    return d->profileMap[name];
}

QList<const KoColorProfile *>  KoColorSpaceRegistry::profilesFor(const QString &id)
{
    return profilesFor(value(id));
}

QList<const KoColorProfile *>  KoColorSpaceRegistry::profilesFor(KoColorSpaceFactory * csf)
{
    QList<const KoColorProfile *>  profiles;

    QHash<QString, KoColorProfile * >::Iterator it;
    for (it = d->profileMap.begin(); it != d->profileMap.end(); ++it) {
        KoColorProfile *  profile = it.value();
        if(csf->profileIsCompatible(profile))
        {
//         if (profile->colorSpaceSignature() == csf->colorSpaceSignature()) {
            profiles.push_back(profile);
        }
    }
    return profiles;
}

void KoColorSpaceRegistry::addProfile(KoColorProfile *p)
{
      if (p->valid()) {
          d->profileMap[p->name()] = p;
      }
}

void KoColorSpaceRegistry::addPaintDeviceAction(const KoColorSpace* cs,
        KisPaintDeviceAction* action) {
    d->paintDevActionMap[cs->id()].append(action);
}

QList<KisPaintDeviceAction *>
KoColorSpaceRegistry::paintDeviceActionsFor(const KoColorSpace* cs) {
    return d->paintDevActionMap[cs->id()];
}

bool KoColorSpaceRegistry::isCached(QString csId, QString profileName) const
{
    return not (d->csMap.find(idsToCacheName(csId, profileName)) == d->csMap.end());
}

QString KoColorSpaceRegistry::idsToCacheName(QString csId, QString profileName) const
{
  return csId + "<comb>" + profileName;
}

const KoColorSpace * KoColorSpaceRegistry::colorSpace(const QString &csID, const QString &pName)
{
    QString profileName = pName;

    if(profileName.isEmpty())
    {
        KoColorSpaceFactory *csf = value(csID);

        if(!csf)
            return 0;

        profileName = csf->defaultProfile();
    }

    QString name = idsToCacheName(csID, profileName);

    if (not isCached(csID, profileName)) {
        KoColorSpaceFactory *csf = value(csID);
        if(!csf)
        {
            kDebug(DBG_PIGMENT) <<"Unknown color space type :" << csf;
            return 0;
        }

        const KoColorProfile *p = profileByName(profileName);
        if(!p && !profileName.isEmpty())
        {
            kDebug(DBG_PIGMENT) <<"Profile not found :" << profileName;
            return 0;
        }
        const KoColorSpace *cs = csf->createColorSpace( p);
        if(!cs)
            return 0;

        d->csMap[name] = cs;
    }

    if(d->csMap.contains(name))
        return d->csMap[name];
    else
        return 0;
}


const KoColorSpace * KoColorSpaceRegistry::colorSpace(const QString &csID, const KoColorProfile *profile)
{
    if( profile )
    {
        const KoColorSpace *cs = 0;
        if(isCached( csID, profile->name() ) )
        {
            cs = colorSpace( csID, profile->name());
        }

        if(!cs)
        {
            // The profile was not stored and thus not the combination either
            KoColorSpaceFactory *csf = value(csID);
            if(!csf)
            {
                kDebug(DBG_PIGMENT) <<"Unknown color space type :" << csf;
                return 0;
            }

            cs = csf->createColorSpace( profile);
            if(!cs )
                return 0;

            QString name = csID + "<comb>" + profile->name();
            d->csMap[name] = cs;
        }

        return cs;
    } else {
        return colorSpace( csID, "");
    }
}

const KoColorSpace * KoColorSpaceRegistry::alpha8()
{
   return d->alphaCs;
}

const KoColorSpace * KoColorSpaceRegistry::rgb8(const QString &profileName)
{
    if( profileName == "" )
    {
        if(not d->rgbU8sRGB)
        {
            d->rgbU8sRGB = colorSpace(KoRgbU8ColorSpace::colorSpaceId(), profileName);
        }
        return d->rgbU8sRGB;
    }
    return colorSpace(KoRgbU8ColorSpace::colorSpaceId(), profileName);
}

const KoColorSpace * KoColorSpaceRegistry::rgb8(const KoColorProfile * profile)
{
    if(profile == 0 )
    {
        if(not d->rgbU8sRGB)
        {
            d->rgbU8sRGB = colorSpace(KoRgbU8ColorSpace::colorSpaceId(), profile);
        }
        return d->rgbU8sRGB;
    }
    return colorSpace(KoRgbU8ColorSpace::colorSpaceId(), profile);
}

const KoColorSpace * KoColorSpaceRegistry::rgb16(const QString &profileName)
{
    return colorSpace(KoRgbU16ColorSpace::colorSpaceId(), profileName);
}

const KoColorSpace * KoColorSpaceRegistry::rgb16(const KoColorProfile * profile)
{
    return colorSpace(KoRgbU16ColorSpace::colorSpaceId(), profile);
}

const KoColorSpace * KoColorSpaceRegistry::lab16(const QString &profileName)
{
    if( profileName == "" )
    {
        if(not d->lab16sLAB)
        {
            d->lab16sLAB = colorSpace(KoRgbU8ColorSpace::colorSpaceId(), profileName);
        }
        return d->lab16sLAB;
    }
    return colorSpace(KoLabColorSpace::colorSpaceId(), profileName);
}

const KoColorSpace * KoColorSpaceRegistry::lab16(const KoColorProfile * profile)
{
    if( profile == 0 )
    {
        if(not d->lab16sLAB)
        {
            d->lab16sLAB = colorSpace(KoRgbU8ColorSpace::colorSpaceId(), profile);
        }
        return d->lab16sLAB;
    }
    return colorSpace(KoLabColorSpace::colorSpaceId(), profile);
}

QList<KoID> KoColorSpaceRegistry::colorModelsList(ColorSpaceListVisibility option ) const
{
    QList<KoID> ids;
    QList<KoColorSpaceFactory*> factories = values();
    foreach(KoColorSpaceFactory* factory, factories)
    {
        if(not ids.contains(factory->colorModelId())
           and ( option == AllColorSpaces or factory->userVisible() ) )
        {
            ids << factory->colorModelId();
        }
    }
    return ids;
}
QList<KoID> KoColorSpaceRegistry::colorDepthList(const KoID& colorModelId, ColorSpaceListVisibility option ) const
{
    return colorDepthList(colorModelId.id(), option);
}


QList<KoID> KoColorSpaceRegistry::colorDepthList(QString colorModelId, ColorSpaceListVisibility option ) const
{
    QList<KoID> ids;
    QList<KoColorSpaceFactory*> factories = values();
    foreach(KoColorSpaceFactory* factory, factories)
    {
        if(not ids.contains(KoID(factory->colorDepthId()))
           and factory->colorModelId().id() == colorModelId
           and ( option == AllColorSpaces or factory->userVisible() ))
        {
            ids << factory->colorDepthId();
        }
    }
    return ids;
}

QString KoColorSpaceRegistry::colorSpaceId(QString colorModelId, QString colorDepthId)
{
    QList<KoColorSpaceFactory*> factories = values();
    foreach(KoColorSpaceFactory* factory, factories)
    {
        if(factory->colorModelId().id() == colorModelId and factory->colorDepthId().id() == colorDepthId )
        {
            return factory->id();
        }
    }
    return "";
}


QString KoColorSpaceRegistry::colorSpaceId(const KoID& colorModelId, const KoID& colorDepthId)
{
    return colorSpaceId( colorModelId.id(), colorDepthId.id());
}

const KoColorConversionSystem* KoColorSpaceRegistry::colorConversionSystem() const
{
    return d->colorConversionSystem;
}

KoColorConversionCache* KoColorSpaceRegistry::colorConversionCache() const
{
    return d->colorConversionCache;
}

#include "KoColorSpaceRegistry.moc"
