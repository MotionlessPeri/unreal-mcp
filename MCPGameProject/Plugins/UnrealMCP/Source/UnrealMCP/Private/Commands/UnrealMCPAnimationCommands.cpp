#include "Commands/UnrealMCPAnimationCommands.h"

#include "Commands/UnrealMCPCommonUtils.h"
#include "Animation/AnimCompositeBase.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequenceBase.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"

namespace
{
void AddAssetPathCandidate(TArray<FString>& OutCandidates, const FString& InPath)
{
    FString Path = InPath;
    Path.TrimStartAndEndInline();
    if (Path.IsEmpty())
    {
        return;
    }

    OutCandidates.AddUnique(Path);

    const int32 FirstQuoteIndex = Path.Find(TEXT("'"));
    const int32 LastQuoteIndex = Path.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
    if (FirstQuoteIndex != INDEX_NONE && LastQuoteIndex != INDEX_NONE && LastQuoteIndex > FirstQuoteIndex)
    {
        const FString QuotedPath = Path.Mid(FirstQuoteIndex + 1, LastQuoteIndex - FirstQuoteIndex - 1);
        if (!QuotedPath.IsEmpty())
        {
            OutCandidates.AddUnique(QuotedPath);
        }
    }

    FString ObjectPath = Path;
    if (ObjectPath.Contains(TEXT("'")))
    {
        const int32 StartQuoteIndex = ObjectPath.Find(TEXT("'"));
        const int32 EndQuoteIndex = ObjectPath.Find(TEXT("'"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
        if (StartQuoteIndex != INDEX_NONE && EndQuoteIndex > StartQuoteIndex)
        {
            ObjectPath = ObjectPath.Mid(StartQuoteIndex + 1, EndQuoteIndex - StartQuoteIndex - 1);
        }
    }

    if (ObjectPath.StartsWith(TEXT("/")))
    {
        OutCandidates.AddUnique(ObjectPath);

        FString PackagePath;
        if (ObjectPath.Split(TEXT("."), &PackagePath, nullptr, ESearchCase::CaseSensitive, ESearchDir::FromStart))
        {
            OutCandidates.AddUnique(PackagePath);
        }
        else
        {
            const FString AssetName = FPackageName::GetShortName(ObjectPath);
            if (!AssetName.IsEmpty())
            {
                OutCandidates.AddUnique(ObjectPath + TEXT(".") + AssetName);
            }
        }
    }
}

UAnimMontage* LoadMontageAsset(const FString& AssetPath, FString& OutResolvedAssetPath)
{
    TArray<FString> CandidatePaths;
    AddAssetPathCandidate(CandidatePaths, AssetPath);

    for (const FString& CandidatePath : CandidatePaths)
    {
        if (UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *CandidatePath))
        {
            OutResolvedAssetPath = Montage->GetPathName();
            return Montage;
        }

        if (UObject* LoadedAsset = UEditorAssetLibrary::LoadAsset(CandidatePath))
        {
            if (UAnimMontage* Montage = Cast<UAnimMontage>(LoadedAsset))
            {
                OutResolvedAssetPath = Montage->GetPathName();
                return Montage;
            }
        }
    }

    return nullptr;
}

FString GetMontageTickTypeName(const FAnimNotifyEvent& NotifyEvent)
{
    switch (NotifyEvent.MontageTickType)
    {
    case EMontageNotifyTickType::Queued:
        return TEXT("Queued");
    case EMontageNotifyTickType::BranchingPoint:
        return TEXT("BranchingPoint");
    default:
        return TEXT("Unknown");
    }
}

FString GetBranchingPointEventTypeName(EAnimNotifyEventType::Type EventType)
{
    switch (EventType)
    {
    case EAnimNotifyEventType::Begin:
        return TEXT("Begin");
    case EAnimNotifyEventType::End:
        return TEXT("End");
    default:
        return TEXT("Unknown");
    }
}

FString GetNotifyKind(const FAnimNotifyEvent& NotifyEvent)
{
    if (NotifyEvent.IsBranchingPoint())
    {
        return TEXT("branching_point");
    }

    if (NotifyEvent.NotifyStateClass)
    {
        return TEXT("notify_state");
    }

    return TEXT("notify");
}

FString GetNotifyDisplayName(const FAnimNotifyEvent& NotifyEvent)
{
    if (NotifyEvent.Notify)
    {
        return NotifyEvent.Notify->GetNotifyName();
    }

    if (NotifyEvent.NotifyStateClass)
    {
        return NotifyEvent.NotifyStateClass->GetNotifyName();
    }

    if (!NotifyEvent.NotifyName.IsNone())
    {
        return NotifyEvent.NotifyName.ToString();
    }

    return NotifyEvent.GetNotifyEventName().ToString();
}

FString GetNotifyClassName(const FAnimNotifyEvent& NotifyEvent)
{
    if (NotifyEvent.Notify)
    {
        return NotifyEvent.Notify->GetClass()->GetName();
    }

    if (NotifyEvent.NotifyStateClass)
    {
        return NotifyEvent.NotifyStateClass->GetClass()->GetName();
    }

    return TEXT("");
}

FString GetNotifyStateClassName(const FAnimNotifyEvent& NotifyEvent)
{
    if (NotifyEvent.NotifyStateClass)
    {
        return NotifyEvent.NotifyStateClass->GetClass()->GetName();
    }

    return TEXT("");
}

FString GetNotifyTrackName(const UAnimMontage* Montage, int32 TrackIndex)
{
#if WITH_EDITORONLY_DATA
    if (Montage && Montage->AnimNotifyTracks.IsValidIndex(TrackIndex))
    {
        return Montage->AnimNotifyTracks[TrackIndex].TrackName.ToString();
    }
#endif
    return TEXT("");
}

FString FindSectionNameForTime(const UAnimMontage* Montage, float Time)
{
    if (!Montage)
    {
        return TEXT("");
    }

    const int32 SectionCount = Montage->CompositeSections.Num();
    for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
    {
        const FCompositeSection& Section = Montage->CompositeSections[SectionIndex];
        const float SectionStartTime = Section.GetTime();
        float SectionEndTime = Montage->GetPlayLength();

        if (SectionIndex + 1 < SectionCount)
        {
            SectionEndTime = Montage->CompositeSections[SectionIndex + 1].GetTime();
        }

        const bool bIsLastSection = SectionIndex + 1 == SectionCount;
        const bool bContainsTime = bIsLastSection
            ? Time >= SectionStartTime && Time <= SectionEndTime + KINDA_SMALL_NUMBER
            : Time >= SectionStartTime && Time < SectionEndTime;

        if (bContainsTime)
        {
            return Section.SectionName.ToString();
        }
    }

    return TEXT("");
}

TSharedPtr<FJsonObject> SerializeSegment(const FAnimSegment& Segment, int32 SegmentIndex)
{
    TSharedPtr<FJsonObject> SegmentObject = MakeShared<FJsonObject>();
    SegmentObject->SetNumberField(TEXT("segment_index"), SegmentIndex);
    SegmentObject->SetNumberField(TEXT("start_pos"), Segment.StartPos);
    SegmentObject->SetNumberField(TEXT("end_pos"), Segment.GetEndPos());
    SegmentObject->SetNumberField(TEXT("segment_length"), Segment.GetLength());
    SegmentObject->SetNumberField(TEXT("anim_start_time"), Segment.AnimStartTime);
    SegmentObject->SetNumberField(TEXT("anim_end_time"), Segment.AnimEndTime);
    SegmentObject->SetNumberField(TEXT("anim_play_rate"), Segment.AnimPlayRate);
    SegmentObject->SetNumberField(TEXT("looping_count"), Segment.LoopingCount);

    if (UAnimSequenceBase* AnimReference = Segment.GetAnimReference())
    {
        SegmentObject->SetStringField(TEXT("linked_sequence"), AnimReference->GetPathName());
        SegmentObject->SetStringField(TEXT("linked_sequence_name"), AnimReference->GetName());
    }
    else
    {
        SegmentObject->SetStringField(TEXT("linked_sequence"), TEXT(""));
        SegmentObject->SetStringField(TEXT("linked_sequence_name"), TEXT(""));
    }

    return SegmentObject;
}

void AddSegmentContextForTime(
    const UAnimMontage* Montage,
    float Time,
    TSharedPtr<FJsonObject>& NotifyObject)
{
    NotifyObject->SetStringField(TEXT("slot_name"), TEXT(""));
    NotifyObject->SetNumberField(TEXT("slot_index"), -1);
    NotifyObject->SetNumberField(TEXT("segment_index"), -1);
    NotifyObject->SetStringField(TEXT("linked_sequence"), TEXT(""));
    NotifyObject->SetStringField(TEXT("linked_sequence_name"), TEXT(""));
    NotifyObject->SetNumberField(TEXT("segment_start"), -1.0);
    NotifyObject->SetNumberField(TEXT("segment_end"), -1.0);

    if (!Montage)
    {
        return;
    }

    for (int32 SlotIndex = 0; SlotIndex < Montage->SlotAnimTracks.Num(); ++SlotIndex)
    {
        const FSlotAnimationTrack& SlotTrack = Montage->SlotAnimTracks[SlotIndex];
        const int32 SegmentIndex = SlotTrack.AnimTrack.GetSegmentIndexAtTime(Time);
        if (!SlotTrack.AnimTrack.AnimSegments.IsValidIndex(SegmentIndex))
        {
            continue;
        }

        const FAnimSegment& Segment = SlotTrack.AnimTrack.AnimSegments[SegmentIndex];
        NotifyObject->SetStringField(TEXT("slot_name"), SlotTrack.SlotName.ToString());
        NotifyObject->SetNumberField(TEXT("slot_index"), SlotIndex);
        NotifyObject->SetNumberField(TEXT("segment_index"), SegmentIndex);
        NotifyObject->SetNumberField(TEXT("segment_start"), Segment.StartPos);
        NotifyObject->SetNumberField(TEXT("segment_end"), Segment.GetEndPos());

        if (UAnimSequenceBase* AnimReference = Segment.GetAnimReference())
        {
            NotifyObject->SetStringField(TEXT("linked_sequence"), AnimReference->GetPathName());
            NotifyObject->SetStringField(TEXT("linked_sequence_name"), AnimReference->GetName());
        }

        return;
    }
}

TSharedPtr<FJsonObject> SerializeNotify(
    const UAnimMontage* Montage,
    const FAnimNotifyEvent& NotifyEvent,
    int32 NotifyIndex)
{
    TSharedPtr<FJsonObject> NotifyObject = MakeShared<FJsonObject>();
    const float TriggerTime = NotifyEvent.GetTriggerTime();
    const float EndTriggerTime = NotifyEvent.GetEndTriggerTime();
    const float Duration = NotifyEvent.GetDuration();

    NotifyObject->SetNumberField(TEXT("notify_index"), NotifyIndex);
    NotifyObject->SetStringField(TEXT("notify_kind"), GetNotifyKind(NotifyEvent));
    NotifyObject->SetStringField(TEXT("name"), GetNotifyDisplayName(NotifyEvent));
    NotifyObject->SetStringField(TEXT("class"), GetNotifyClassName(NotifyEvent));
    NotifyObject->SetStringField(TEXT("notify_state_class"), GetNotifyStateClassName(NotifyEvent));
    NotifyObject->SetNumberField(TEXT("trigger_time"), TriggerTime);
    NotifyObject->SetNumberField(TEXT("duration"), Duration);
    NotifyObject->SetNumberField(TEXT("end_time"), EndTriggerTime);
    NotifyObject->SetStringField(TEXT("track_name"), GetNotifyTrackName(Montage, NotifyEvent.TrackIndex));
    NotifyObject->SetNumberField(TEXT("track_index"), NotifyEvent.TrackIndex);
    NotifyObject->SetBoolField(TEXT("is_branching_point"), NotifyEvent.IsBranchingPoint());
    NotifyObject->SetBoolField(TEXT("converted_from_branching_point"), NotifyEvent.bConvertedFromBranchingPoint);
    NotifyObject->SetStringField(TEXT("montage_tick_type"), GetMontageTickTypeName(NotifyEvent));
    NotifyObject->SetStringField(TEXT("section_name"), FindSectionNameForTime(Montage, TriggerTime));
#if WITH_EDITORONLY_DATA
    NotifyObject->SetStringField(TEXT("notify_guid"), NotifyEvent.Guid.ToString(EGuidFormats::DigitsWithHyphensLower));
#else
    NotifyObject->SetStringField(TEXT("notify_guid"), TEXT(""));
#endif

    AddSegmentContextForTime(Montage, TriggerTime, NotifyObject);
    return NotifyObject;
}

TSharedPtr<FJsonObject> SerializeBranchingPoint(
    const UAnimMontage* Montage,
    const FAnimNotifyEvent& NotifyEvent,
    int32 NotifyIndex,
    float TriggerTime,
    EAnimNotifyEventType::Type EventType)
{
    TSharedPtr<FJsonObject> MarkerObject = MakeShared<FJsonObject>();
    MarkerObject->SetNumberField(TEXT("notify_index"), NotifyIndex);
    MarkerObject->SetNumberField(TEXT("trigger_time"), TriggerTime);
    MarkerObject->SetStringField(TEXT("event_type"), GetBranchingPointEventTypeName(EventType));
    MarkerObject->SetStringField(TEXT("name"), GetNotifyDisplayName(NotifyEvent));
    MarkerObject->SetStringField(TEXT("class"), GetNotifyClassName(NotifyEvent));
    MarkerObject->SetStringField(TEXT("track_name"), GetNotifyTrackName(Montage, NotifyEvent.TrackIndex));
    MarkerObject->SetNumberField(TEXT("track_index"), NotifyEvent.TrackIndex);
    MarkerObject->SetStringField(TEXT("section_name"), FindSectionNameForTime(Montage, TriggerTime));
    AddSegmentContextForTime(Montage, TriggerTime, MarkerObject);

    return MarkerObject;
}
}

FUnrealMCPAnimationCommands::FUnrealMCPAnimationCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPAnimationCommands::HandleCommand(
    const FString& CommandType,
    const TSharedPtr<FJsonObject>& Params)
{
    if (CommandType == TEXT("get_montage_info"))
    {
        return HandleGetMontageInfo(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(
        FString::Printf(TEXT("Unknown animation command: %s"), *CommandType));
}

TSharedPtr<FJsonObject> FUnrealMCPAnimationCommands::HandleGetMontageInfo(const TSharedPtr<FJsonObject>& Params)
{
    FString AssetPath;
    if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            TEXT("Missing 'asset_path' parameter (e.g. \"/Game/Animations/MyMontage\")"));
    }

    FString ResolvedAssetPath;
    UAnimMontage* Montage = LoadMontageAsset(AssetPath, ResolvedAssetPath);
    if (!Montage)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Could not load AnimMontage asset at path: %s"), *AssetPath));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("asset_path"), ResolvedAssetPath);
    Result->SetStringField(TEXT("asset_name"), Montage->GetName());
    Result->SetNumberField(TEXT("play_length"), Montage->GetPlayLength());
    Result->SetNumberField(TEXT("rate_scale"), Montage->RateScale);

    TArray<TSharedPtr<FJsonValue>> Sections;
    for (int32 SectionIndex = 0; SectionIndex < Montage->CompositeSections.Num(); ++SectionIndex)
    {
        const FCompositeSection& Section = Montage->CompositeSections[SectionIndex];
        TSharedPtr<FJsonObject> SectionObject = MakeShared<FJsonObject>();
        SectionObject->SetNumberField(TEXT("section_index"), SectionIndex);
        SectionObject->SetStringField(TEXT("name"), Section.SectionName.ToString());
        SectionObject->SetStringField(TEXT("next_section_name"), Section.NextSectionName.ToString());
        SectionObject->SetNumberField(TEXT("start_time"), Section.GetTime());
        SectionObject->SetNumberField(TEXT("length"), Montage->GetSectionLength(SectionIndex));
        SectionObject->SetNumberField(TEXT("end_time"), Section.GetTime() + Montage->GetSectionLength(SectionIndex));
        Sections.Add(MakeShared<FJsonValueObject>(SectionObject));
    }
    Result->SetArrayField(TEXT("sections"), Sections);

    TArray<TSharedPtr<FJsonValue>> SlotTracks;
    for (int32 SlotIndex = 0; SlotIndex < Montage->SlotAnimTracks.Num(); ++SlotIndex)
    {
        const FSlotAnimationTrack& SlotTrack = Montage->SlotAnimTracks[SlotIndex];
        TSharedPtr<FJsonObject> SlotObject = MakeShared<FJsonObject>();
        SlotObject->SetNumberField(TEXT("slot_index"), SlotIndex);
        SlotObject->SetStringField(TEXT("slot_name"), SlotTrack.SlotName.ToString());

        TArray<TSharedPtr<FJsonValue>> Segments;
        for (int32 SegmentIndex = 0; SegmentIndex < SlotTrack.AnimTrack.AnimSegments.Num(); ++SegmentIndex)
        {
            Segments.Add(MakeShared<FJsonValueObject>(
                SerializeSegment(SlotTrack.AnimTrack.AnimSegments[SegmentIndex], SegmentIndex)));
        }

        SlotObject->SetNumberField(TEXT("segment_count"), SlotTrack.AnimTrack.AnimSegments.Num());
        SlotObject->SetArrayField(TEXT("segments"), Segments);
        SlotTracks.Add(MakeShared<FJsonValueObject>(SlotObject));
    }
    Result->SetArrayField(TEXT("slot_tracks"), SlotTracks);

    TArray<TSharedPtr<FJsonValue>> Notifies;
    for (int32 NotifyIndex = 0; NotifyIndex < Montage->Notifies.Num(); ++NotifyIndex)
    {
        Notifies.Add(MakeShared<FJsonValueObject>(
            SerializeNotify(Montage, Montage->Notifies[NotifyIndex], NotifyIndex)));
    }
    Result->SetArrayField(TEXT("notifies"), Notifies);

    TArray<TSharedPtr<FJsonValue>> BranchingPoints;
    for (int32 NotifyIndex = 0; NotifyIndex < Montage->Notifies.Num(); ++NotifyIndex)
    {
        const FAnimNotifyEvent& NotifyEvent = Montage->Notifies[NotifyIndex];
        if (!NotifyEvent.IsBranchingPoint())
        {
            continue;
        }

        BranchingPoints.Add(MakeShared<FJsonValueObject>(SerializeBranchingPoint(
            Montage,
            NotifyEvent,
            NotifyIndex,
            NotifyEvent.GetTriggerTime(),
            EAnimNotifyEventType::Begin)));

        if (NotifyEvent.NotifyStateClass && NotifyEvent.GetDuration() > 0.0f)
        {
            BranchingPoints.Add(MakeShared<FJsonValueObject>(SerializeBranchingPoint(
                Montage,
                NotifyEvent,
                NotifyIndex,
                NotifyEvent.GetEndTriggerTime(),
                EAnimNotifyEventType::End)));
        }
    }
    Result->SetArrayField(TEXT("branching_points"), BranchingPoints);

    return Result;
}

TArray<FMCPCommandMeta> FUnrealMCPAnimationCommands::GetCommandMetadata()
{
	return {
		{TEXT("get_montage_info"), TEXT("animation"), TEXT("Read an AnimMontage asset structure"), {
			{TEXT("asset_path"), TEXT("string"), true, TEXT("Path to AnimMontage asset")}
		}}
	};
}
