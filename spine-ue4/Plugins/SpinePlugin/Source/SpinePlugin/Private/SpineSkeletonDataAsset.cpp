/******************************************************************************
 * Spine Runtimes Software License v2.5
 *
 * Copyright (c) 2013-2016, Esoteric Software
 * All rights reserved.
 *
 * You are granted a perpetual, non-exclusive, non-sublicensable, and
 * non-transferable license to use, install, execute, and perform the Spine
 * Runtimes software and derivative works solely for personal or internal
 * use. Without the written permission of Esoteric Software (see Section 2 of
 * the Spine Software License Agreement), you may not (a) modify, translate,
 * adapt, or develop new applications using the Spine Runtimes or otherwise
 * create derivative works or improvements of the Spine Runtimes or (b) remove,
 * delete, alter, or obscure any trademarks or any copyright, trademark, patent,
 * or other intellectual property or proprietary rights notices on or in the
 * Software, including any copy thereof. Redistributions in binary or source
 * form must include this license and terms.
 *
 * THIS SOFTWARE IS PROVIDED BY ESOTERIC SOFTWARE "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL ESOTERIC SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, BUSINESS INTERRUPTION, OR LOSS OF
 * USE, DATA, OR PROFITS) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *****************************************************************************/

#include "SpinePluginPrivatePCH.h"
#include "spine/spine.h"
#include <string.h>
#include <string>
#include <stdlib.h>
#include "Runtime/Core/Public/Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "Spine"

using namespace spine;

FName USpineSkeletonDataAsset::GetSkeletonDataFileName () const {
#if WITH_EDITORONLY_DATA
	TArray<FString> files;
	if (importData) importData->ExtractFilenames(files);
	if (files.Num() > 0) return FName(*files[0]);
	else return skeletonDataFileName;
#else
	return skeletonDataFileName;
#endif
}

#if WITH_EDITORONLY_DATA

void USpineSkeletonDataAsset::SetSkeletonDataFileName (const FName &SkeletonDataFileName) {
	importData->UpdateFilenameOnly(SkeletonDataFileName.ToString());
	TArray<FString> files;
	importData->ExtractFilenames(files);
	if (files.Num() > 0) this->skeletonDataFileName = FName(*files[0]);
}

void USpineSkeletonDataAsset::PostInitProperties () {
	if (!HasAnyFlags(RF_ClassDefaultObject)) importData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	Super::PostInitProperties();
}

void USpineSkeletonDataAsset::GetAssetRegistryTags (TArray<FAssetRegistryTag>& OutTags) const {
	if (importData) {
		OutTags.Add(FAssetRegistryTag(SourceFileTagName(), importData->GetSourceData().ToJson(), FAssetRegistryTag::TT_Hidden) );
	}
	
	Super::GetAssetRegistryTags(OutTags);
}

void USpineSkeletonDataAsset::Serialize (FArchive& Ar) {
	Super::Serialize(Ar);
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_ASSET_IMPORT_DATA_AS_JSON && !importData)
		importData = NewObject<UAssetImportData>(this, TEXT("AssetImportData"));
	LoadInfo();
}

#endif

void USpineSkeletonDataAsset::BeginDestroy () {
	if (this->skeletonData) {
		delete this->skeletonData;
		this->skeletonData = nullptr;
	}
	if (this->animationStateData) {
		delete this->animationStateData;
		this->animationStateData = nullptr;
	}
	Super::BeginDestroy();
}

class SP_API NullAttachmentLoader : public AttachmentLoader {
public:

	virtual RegionAttachment* newRegionAttachment(Skin& skin, const String& name, const String& path) {
		return new(__FILE__, __LINE__) RegionAttachment(name);
	}

	virtual MeshAttachment* newMeshAttachment(Skin& skin, const String& name, const String& path) {
		return new(__FILE__, __LINE__) MeshAttachment(name);
	}

	virtual BoundingBoxAttachment* newBoundingBoxAttachment(Skin& skin, const String& name) {
		return new(__FILE__, __LINE__) BoundingBoxAttachment(name);
	}

	virtual PathAttachment* newPathAttachment(Skin& skin, const String& name) {
		return new(__FILE__, __LINE__) PathAttachment(name);
	}

	virtual PointAttachment* newPointAttachment(Skin& skin, const String& name) {
		return new(__FILE__, __LINE__) PointAttachment(name);
	}

	virtual ClippingAttachment* newClippingAttachment(Skin& skin, const String& name) {
		return new(__FILE__, __LINE__) ClippingAttachment(name);
	}

	virtual void configureAttachment(Attachment* attachment) {

	}
};

void USpineSkeletonDataAsset::LoadInfo() {
#if WITH_EDITORONLY_DATA
	int dataLen = rawData.Num();
	if (dataLen == 0) return;
	NullAttachmentLoader loader;
	SkeletonData* skeletonData = nullptr;
	if (skeletonDataFileName.GetPlainNameString().Contains(TEXT(".json"))) {
		SkeletonJson* json = new (__FILE__, __LINE__) SkeletonJson(&loader);
		skeletonData = json->readSkeletonData((const char*)rawData.GetData());
		if (!skeletonData) {
			FMessageDialog::Debugf(FText::FromString(UTF8_TO_TCHAR(json->getError().buffer())));
			UE_LOG(SpineLog, Error, TEXT("Couldn't load skeleton data and atlas: %s"), UTF8_TO_TCHAR(json->getError().buffer()));
		}
		delete json;
	} else {
		SkeletonBinary* binary = new (__FILE__, __LINE__) SkeletonBinary(&loader);
		skeletonData = binary->readSkeletonData((const unsigned char*)rawData.GetData(), (int)rawData.Num());
		if (!skeletonData) {
			FMessageDialog::Debugf(FText::FromString(UTF8_TO_TCHAR(binary->getError().buffer())));
			UE_LOG(SpineLog, Error, TEXT("Couldn't load skeleton data and atlas: %s"), UTF8_TO_TCHAR(binary->getError().buffer()));
		}
		delete binary;
	}
	if (skeletonData) {
		Bones.Empty();
		for (int i = 0; i < skeletonData->getBones().size(); i++)
			Bones.Add(skeletonData->getBones()[i]->getName().buffer());
		Skins.Empty();
		for (int i = 0; i < skeletonData->getSkins().size(); i++)
			Skins.Add(skeletonData->getSkins()[i]->getName().buffer());
		Slots.Empty();
		for (int i = 0; i < skeletonData->getSlots().size(); i++)
			Slots.Add(skeletonData->getSlots()[i]->getName().buffer());
		Animations.Empty();
		for (int i = 0; i < skeletonData->getAnimations().size(); i++)
			Animations.Add(skeletonData->getAnimations()[i]->getName().buffer());
		Events.Empty();
		for (int i = 0; i < skeletonData->getEvents().size(); i++)
			Events.Add(skeletonData->getEvents()[i]->getName().buffer());
		delete skeletonData;
	}
#endif
}

void USpineSkeletonDataAsset::SetRawData(TArray<uint8> &Data) {
	this->rawData.Empty();
	this->rawData.Append(Data);

	if (skeletonData) {
		delete skeletonData;
		skeletonData = nullptr;
	}

	LoadInfo();
}

SkeletonData* USpineSkeletonDataAsset::GetSkeletonData (Atlas* Atlas) {
	if (!skeletonData || lastAtlas != Atlas) {
		if (skeletonData) {
			delete skeletonData;
			skeletonData = nullptr;
		}		
		int dataLen = rawData.Num();
		if (skeletonDataFileName.GetPlainNameString().Contains(TEXT(".json"))) {
			SkeletonJson* json = new (__FILE__, __LINE__) SkeletonJson(Atlas);
			this->skeletonData = json->readSkeletonData((const char*)rawData.GetData());
			if (!skeletonData) {
#if WITH_EDITORONLY_DATA
				FMessageDialog::Debugf(FText::FromString(UTF8_TO_TCHAR(json->getError().buffer())));
#endif
				UE_LOG(SpineLog, Error, TEXT("Couldn't load skeleton data and atlas: %s"), UTF8_TO_TCHAR(json->getError().buffer()));
			}
			delete json;
		} else {
			SkeletonBinary* binary = new (__FILE__, __LINE__) SkeletonBinary(Atlas);
			this->skeletonData = binary->readSkeletonData((const unsigned char*)rawData.GetData(), (int)rawData.Num());
			if (!skeletonData) {
#if WITH_EDITORONLY_DATA
				FMessageDialog::Debugf(FText::FromString(UTF8_TO_TCHAR(binary->getError().buffer())));
#endif
				UE_LOG(SpineLog, Error, TEXT("Couldn't load skeleton data and atlas: %s"), UTF8_TO_TCHAR(binary->getError().buffer()));
			}
			delete binary;
		}
		if (animationStateData) {
			delete animationStateData;
			animationStateData = nullptr;
			GetAnimationStateData(Atlas);
		}
		lastAtlas = Atlas;
	}
	return this->skeletonData;
}

AnimationStateData* USpineSkeletonDataAsset::GetAnimationStateData(Atlas* atlas) {
	if (!animationStateData) {
		SkeletonData* data = GetSkeletonData(atlas);
		animationStateData = new (__FILE__, __LINE__) AnimationStateData(data);
	}
	for (auto& data : MixData) {
		if (!data.From.IsEmpty() && !data.To.IsEmpty()) {
			const char* fromChar = TCHAR_TO_UTF8(*data.From);
			const char* toChar = TCHAR_TO_UTF8(*data.To);
			animationStateData->setMix(fromChar, toChar, data.Mix);
		}
	}
	animationStateData->setDefaultMix(DefaultMix);
	return this->animationStateData;
}

void USpineSkeletonDataAsset::SetMix(const FString& from, const FString& to, float mix) {
	FSpineAnimationStateMixData data;
	data.From = from;
	data.To = to;
	data.Mix = mix;	
	this->MixData.Add(data);
	if (lastAtlas) {
		GetAnimationStateData(lastAtlas);
	}
}

float USpineSkeletonDataAsset::GetMix(const FString& from, const FString& to) {
	for (auto& data : MixData) {
		if (data.From.Equals(from) && data.To.Equals(to)) return data.Mix;
	}
	return 0;
}

#undef LOCTEXT_NAMESPACE
