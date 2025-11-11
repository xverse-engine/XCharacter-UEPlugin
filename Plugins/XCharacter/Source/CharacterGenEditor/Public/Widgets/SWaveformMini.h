// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Rendering/DrawElements.h"

// 前向声明
struct FAudioSegmentInfo;


/**
 * 简易波形绘制控件
 * 用于在TTS预览界面中显示音频波形和片段标记
 */
class CHARACTERGENEDITOR_API SWaveformMini : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SWaveformMini) 
        : _Samples(nullptr)
        , _NumSamples(0)
        , _SampleRate(0) 
        , _PlayProgress(0.0f)
    {}
        SLATE_ARGUMENT(const int16*, Samples)
        SLATE_ARGUMENT(int32, NumSamples)
        SLATE_ARGUMENT(uint32, SampleRate)
        SLATE_ATTRIBUTE(TArray<FAudioSegmentInfo>, Segments)
        SLATE_ATTRIBUTE(float, PlayProgress)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
                         const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, 
                         int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

    virtual FVector2D ComputeDesiredSize(float) const override;

private:
    /**
     * 绘制波形片段标记
     * @param OutDrawElements 绘制元素列表
     * @param LayerId 绘制层级
     * @param AllottedGeometry 分配几何体
     * @param Size 控件大小
     */
    void DrawSegmentMarkers(FSlateWindowElementList& OutDrawElements, int32 LayerId, 
                           const FGeometry& AllottedGeometry, const FVector2D& Size) const;

    /**
     * 绘制时间刻度线
     * @param OutDrawElements 绘制元素列表
     * @param LayerId 绘制层级
     * @param AllottedGeometry 分配几何体
     * @param Size 控件大小
     */
    void DrawTimeScale(FSlateWindowElementList& OutDrawElements, int32 LayerId, 
                      const FGeometry& AllottedGeometry, const FVector2D& Size) const;

private:
    const int16* Samples = nullptr;
    int32 NumSamples = 0;
    uint32 SampleRate = 0;
    TAttribute<TArray<FAudioSegmentInfo>> Segments;
    TAttribute<float> PlayProgress;
};
