// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SWaveformMini.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/SWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/Geometry.h"
#include "Internationalization/Text.h"
#include "Misc/StringBuilder.h"
#include "Logging/LogMacros.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector2D.h"
#include "Math/Color.h"
#include "Rendering/SlateRenderer.h"
#include "Containers/Array.h"
#include "CharacterGenEditorManager.h"
#include "Misc/AssertionMacros.h"
#include "Styling/CoreStyle.h"

void SWaveformMini::Construct(const FArguments& InArgs)
{
    Samples = InArgs._Samples;
    NumSamples = InArgs._NumSamples;
    SampleRate = InArgs._SampleRate;
    Segments = InArgs._Segments;
    PlayProgress = InArgs._PlayProgress;
}

int32 SWaveformMini::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
                            const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, 
                            int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    const FVector2D Size = AllottedGeometry.GetLocalSize();
    if (!Samples || NumSamples <= 0 || Size.X <= 1.f || Size.Y <= 1.f)
    {
        return LayerId;
    }

    // 定义时间刻度区域高度
    const float TimeScaleHeight = 20.0f;
    const float WaveformHeight = Size.Y - TimeScaleHeight;
    const float HalfWaveformHeight = WaveformHeight * 0.5f;

    // 绘制波形
    
    TArray<FVector2D> Points;
    Points.Reserve(FMath::Min<int32>(NumSamples, (int32)Size.X));
    const int32 Step = FMath::Max(1, NumSamples / FMath::Max<int32>(1, (int32)Size.X));
    for (int32 i = 0; i < NumSamples; i += Step)
    {
        const float Norm = (float)Samples[i] / 32768.f;
        const float y = HalfWaveformHeight - Norm * HalfWaveformHeight;
        const float x = (float)i / (float)NumSamples * Size.X;
        Points.Add(FVector2D(x, y));
    }
    FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), 
                                Points, ESlateDrawEffect::None, FLinearColor(0.f, 0.6f, 1.f, 1.f), true, 1.f);
    LayerId++;

    // 绘制时间戳标记线
    if (Segments.IsSet() && Segments.Get().Num() > 0 && SampleRate > 0)
    {
        DrawSegmentMarkers(OutDrawElements, LayerId, AllottedGeometry, Size);
        LayerId++;
    }

    // 绘制播放进度指示器
    if (PlayProgress.IsSet())
    {
        const float Progress = FMath::Clamp(PlayProgress.Get(), 0.0f, 1.0f);
        const float ProgressX = Progress * Size.X;
        
        // 绘制进度线阴影（稍微偏移，创造立体效果）
        TArray<FVector2D> ShadowLine;
        ShadowLine.Add(FVector2D(ProgressX + 1.0f, 0.0f));
        ShadowLine.Add(FVector2D(ProgressX + 1.0f, WaveformHeight));
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), 
                                    ShadowLine, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.3f), true, 2.0f);
        LayerId++;
        
        // 绘制主进度线
        TArray<FVector2D> ProgressLine;
        ProgressLine.Add(FVector2D(ProgressX, 0.0f));
        ProgressLine.Add(FVector2D(ProgressX, WaveformHeight));
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), 
                                    ProgressLine, ESlateDrawEffect::None, FLinearColor(1.0f, 0.2f, 0.2f, 1.0f), true, 2.0f);
        LayerId++;
        
    }

    // 绘制时间刻度线
    if (SampleRate > 0 && NumSamples > 0)
    {
        DrawTimeScale(OutDrawElements, LayerId, AllottedGeometry, Size);
        LayerId++;
    }

    return LayerId;
}

FVector2D SWaveformMini::ComputeDesiredSize(float) const
{
    return FVector2D(800.f, 140.f); // 增加20像素高度用于时间刻度线
}

void SWaveformMini::DrawSegmentMarkers(FSlateWindowElementList& OutDrawElements, int32 LayerId, 
                                      const FGeometry& AllottedGeometry, const FVector2D& Size) const
{
    // 安全检查
    if (!Segments.IsSet())
    {
        UE_LOG(LogTemp, Warning, TEXT("DrawSegmentMarkers: Segments is not set"));
        return;
    }
    
    const TArray<FAudioSegmentInfo>& SegmentsArray = Segments.Get();
    if (SegmentsArray.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("DrawSegmentMarkers: No segments to draw"));
        return;
    }
    
    if (SampleRate == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("DrawSegmentMarkers: Invalid sample rate: %d"), SampleRate);
        return;
    }
    
    if (NumSamples <= 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("DrawSegmentMarkers: Invalid sample count: %d"), NumSamples);
        return;
    }

    const float TotalDuration = (float)NumSamples / (float)SampleRate;
    if (TotalDuration <= 0.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("DrawSegmentMarkers: Invalid duration: %.2f"), TotalDuration);
        return;
    }
    
    // 定义时间刻度区域高度
    const float TimeScaleHeight = 20.0f;
    const float WaveformHeight = Size.Y - TimeScaleHeight;
    
    const float PixelsPerSecond = Size.X / TotalDuration;
    
    // 限制绘制的片段数量，避免性能问题
    const int32 MaxSegmentsToDraw = 50;
    const int32 SegmentsToDraw = FMath::Min(SegmentsArray.Num(), MaxSegmentsToDraw);
    
    // UE_LOG(LogTemp, Log, TEXT("DrawSegmentMarkers: Drawing %d/%d segments, duration=%.2fs, pixelsPerSecond=%.2f"), 
    //        SegmentsToDraw, SegmentsArray.Num(), TotalDuration, PixelsPerSecond);

    // 绘制片段标记
    for (int32 i = 0; i < SegmentsToDraw; ++i)
    {
        
        const FAudioSegmentInfo& Segment = SegmentsArray[i];
        
        // 验证Segment数据的有效性
        if (FMath::IsNaN(Segment.StartTime) || FMath::IsNaN(Segment.EndTime) || 
            Segment.StartTime < 0.0f || Segment.EndTime < Segment.StartTime)
        {
            continue;
        }
        
        // 验证标签文本的有效性
        if (Segment.Label.Len() > 1000) // 防止异常长的文本
        {
            continue;
        }
        
        // 计算标记线的X位置
        float StartX = Segment.StartTime * PixelsPerSecond;
        float EndX = Segment.EndTime * PixelsPerSecond;
        
        // 验证计算结果的有效性
        if (FMath::IsNaN(StartX) || FMath::IsNaN(EndX) || 
            !FMath::IsFinite(StartX) || !FMath::IsFinite(EndX))
        {
            continue;
        }
        
        // 限制位置范围，防止绘制超出边界
        StartX = FMath::Clamp(StartX, 0.0f, Size.X);
        EndX = FMath::Clamp(EndX, 0.0f, Size.X);

        // 确保在可见区域内
        if (StartX >= 0 && StartX <= Size.X)
        {
            // 绘制开始标记线
            TArray<FVector2D> StartLine;
            StartLine.Add(FVector2D(StartX, 0));
            StartLine.Add(FVector2D(StartX, WaveformHeight));
            
            FLinearColor LineColor = Segment.bIsSilence ? 
                FLinearColor(0.8f, 0.8f, 0.8f, 0.7f) :  // 静音区域用灰色
                Segment.Color;
            
            FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), 
                StartLine, ESlateDrawEffect::None, LineColor, true, 2.0f);

            // 绘制标签文本
            if (!Segment.Label.IsEmpty())
            {
                // 限制文本长度，避免显示问题
                FString ShortLabel = Segment.Label.Len() > 30 ? Segment.Label.Left(30) + TEXT("...") : Segment.Label;
                
                // 安全地格式化文本 - 标签已经包含时长信息，不需要重复显示
                FString DisplayText = ShortLabel;
                
                // 计算文本位置 - 根据片段类型调整位置
                FVector2D TextPosition;
                
                // 估算文本宽度（简化版本，避免复杂计算）
                float EstimatedTextWidth = ShortLabel.Len() * 7.0f; // 平均每个字符7像素
                EstimatedTextWidth = FMath::Min(EstimatedTextWidth, 120.0f);
                
                if (Segment.bIsSilence)
                {
                    // 静音区域：文本居中显示在静音区域上方
                    float SegmentWidth = FMath::Max(1.0f, EndX - StartX);
                    
                    // 简化居中算法：如果静音区域足够宽，就居中；否则左对齐
                    if (SegmentWidth >= EstimatedTextWidth + 10) // 有足够空间居中
                    {
                        float CenterX = StartX + SegmentWidth * 0.5f;
                        TextPosition = FVector2D(CenterX - EstimatedTextWidth * 0.5f, 2);
                    }
                    else // 空间不够，左对齐
                    {
                        TextPosition = FVector2D(StartX + 2, 2);
                    }
                }
                else
                {
                    // 音频内容：文本显示在开始位置
                    TextPosition = FVector2D(StartX + 2, 2);
                }
                
                // 确保文本在可见区域内
                if (TextPosition.X < 0)
                {
                    TextPosition.X = 2;
                }
                if (TextPosition.X + EstimatedTextWidth > Size.X)
                {
                    TextPosition.X = FMath::Max(2.0f, Size.X - EstimatedTextWidth);
                }
                
                // 使用估算的文本宽度
                FVector2D TextSize = FVector2D(EstimatedTextWidth, 16.0f);
                if (TextSize.X > 0 && TextSize.Y > 0)
                {
                    FSlateDrawElement::MakeText(OutDrawElements, LayerId, 
                        AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextPosition)),
                        FText::FromString(DisplayText), FCoreStyle::GetDefaultFontStyle("Normal", 8),
                        ESlateDrawEffect::None, LineColor);
                }
            }
        }

        // 绘制结束标记线
        if (EndX >= 0 && EndX <= Size.X && EndX != StartX)
        {
            TArray<FVector2D> EndLine;
            EndLine.Add(FVector2D(EndX, 0));
            EndLine.Add(FVector2D(EndX, WaveformHeight));
            
            FLinearColor LineColor = Segment.bIsSilence ? 
                FLinearColor(0.8f, 0.8f, 0.8f, 0.7f) :  // 静音区域用灰色
                Segment.Color;
            
            FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), 
                EndLine, ESlateDrawEffect::None, LineColor, true, 2.0f);
        }

        // 如果是静音区域，绘制背景填充
        if (Segment.bIsSilence && EndX > StartX)
        {
            // 计算静音区域的几何体
            float SilenceWidth = EndX - StartX;
            if (SilenceWidth > 0 && FMath::IsFinite(SilenceWidth))
            {
                FVector2D SilenceSize = FVector2D(SilenceWidth, WaveformHeight);
                FVector2D SilencePosition = FVector2D(StartX, 0);
                
                // 验证静音区域的有效性
                if (SilenceSize.X > 0 && SilenceSize.Y > 0 && 
                    FMath::IsFinite(SilenceSize.X) && FMath::IsFinite(SilenceSize.Y) &&
                    SilencePosition.X >= 0 && SilencePosition.X <= Size.X)
                {
                    FSlateDrawElement::MakeBox(OutDrawElements, LayerId - 1, 
                        AllottedGeometry.ToPaintGeometry(SilenceSize, FSlateLayoutTransform(SilencePosition)),
                        FAppStyle::GetBrush("WhiteBrush"),
                        ESlateDrawEffect::None,
                        FLinearColor(0.8f, 0.8f, 0.8f, 0.2f)); // 半透明灰色背景
                }
            }
        }
    }
}

void SWaveformMini::DrawTimeScale(FSlateWindowElementList& OutDrawElements, int32 LayerId, 
                                 const FGeometry& AllottedGeometry, const FVector2D& Size) const
{
    if (SampleRate <= 0 || NumSamples <= 0)
    {
        return;
    }

    const float TotalDuration = (float)NumSamples / (float)SampleRate;
    if (TotalDuration <= 0.0f)
    {
        return;
    }

    // 计算时间刻度的间隔
    // 根据总时长选择合适的间隔，最小刻度为0.5秒
    float TimeInterval = 0.5f; // 最小刻度为0.5秒
    
    if (TotalDuration <= 2.0f)
    {
        TimeInterval = 0.5f; // 2秒以内用0.5秒间隔
    }
    else if (TotalDuration <= 10.0f)
    {
        TimeInterval = 1.0f; // 10秒以内用1秒间隔
    }
    else if (TotalDuration <= 30.0f)
    {
        TimeInterval = 2.0f; // 30秒以内用2秒间隔
    }
    else if (TotalDuration <= 120.0f)
    {
        TimeInterval = 5.0f; // 2分钟以内用5秒间隔
    }
    else if (TotalDuration <= 600.0f)
    {
        TimeInterval = 10.0f; // 10分钟以内用10秒间隔
    }
    else
    {
        TimeInterval = 30.0f; // 更长用30秒间隔
    }

    const float PixelsPerSecond = Size.X / TotalDuration;
    const float TimeScaleHeight = 20.0f; // 时间刻度区域高度
    const float TimeScaleY = Size.Y - TimeScaleHeight; // 时间刻度区域Y位置

    // 绘制时间刻度背景
    FSlateDrawElement::MakeBox(OutDrawElements, LayerId, 
        AllottedGeometry.ToPaintGeometry(FVector2D(Size.X, TimeScaleHeight), FSlateLayoutTransform(FVector2D(0, TimeScaleY))),
        FAppStyle::GetBrush("WhiteBrush"),
        ESlateDrawEffect::None,
        FLinearColor(0.95f, 0.95f, 0.95f, 0.8f)); // 浅灰色背景

    // 绘制时间刻度线和标签
    for (float Time = 0.0f; Time <= TotalDuration; Time += TimeInterval)
    {
        const float X = Time * PixelsPerSecond;
        
        // 确保在可见范围内
        if (X >= 0 && X <= Size.X)
        {
            // 绘制刻度线
            TArray<FVector2D> TickLine;
            TickLine.Add(FVector2D(X, TimeScaleY));
            TickLine.Add(FVector2D(X, TimeScaleY + TimeScaleHeight));
            
            FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), 
                TickLine, ESlateDrawEffect::None, FLinearColor(0.3f, 0.3f, 0.3f, 1.0f), true, 1.0f);

            // 绘制时间标签
            FString TimeText;
            if (Time < 60.0f)
            {
                // 小于1分钟，显示秒数
                TimeText = FString::Printf(TEXT("%.1fs"), Time);
            }
            else
            {
                // 大于1分钟，显示分:秒格式
                int32 Minutes = (int32)(Time / 60.0f);
                float Seconds = Time - (Minutes * 60.0f);
                TimeText = FString::Printf(TEXT("%d:%.1f"), Minutes, Seconds);
            }

            // 计算文本位置（居中在刻度线上方）
            FVector2D TextSize = FVector2D(TimeText.Len() * 6.0f, 12.0f); // 估算文本大小
            FVector2D TextPosition = FVector2D(X - TextSize.X * 0.5f, TimeScaleY + 2.0f);
            
            // 确保文本不超出边界
            if (TextPosition.X < 0)
            {
                TextPosition.X = 2.0f;
            }
            else if (TextPosition.X + TextSize.X > Size.X)
            {
                TextPosition.X = Size.X - TextSize.X - 2.0f;
            }

            FSlateDrawElement::MakeText(OutDrawElements, LayerId, 
                AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextPosition)),
                FText::FromString(TimeText), FCoreStyle::GetDefaultFontStyle("Normal", 9),
                ESlateDrawEffect::None, FLinearColor(0.2f, 0.2f, 0.2f, 1.0f));
        }
    }

    // 绘制总时长标签（在右侧）
    FString TotalTimeText;
    if (TotalDuration < 60.0f)
    {
        TotalTimeText = FString::Printf(TEXT("总时长: %.1fs"), TotalDuration);
    }
    else
    {
        int32 TotalMinutes = (int32)(TotalDuration / 60.0f);
        float TotalSeconds = TotalDuration - (TotalMinutes * 60.0f);
        TotalTimeText = FString::Printf(TEXT("总时长: %d:%.1f"), TotalMinutes, TotalSeconds);
    }

    FVector2D TotalTimeSize = FVector2D(TotalTimeText.Len() * 6.0f, 12.0f);
    FVector2D TotalTimePosition = FVector2D(Size.X - TotalTimeSize.X - 5.0f, TimeScaleY + 2.0f);
    
    FSlateDrawElement::MakeText(OutDrawElements, LayerId, 
        AllottedGeometry.ToPaintGeometry(TotalTimeSize, FSlateLayoutTransform(TotalTimePosition)),
        FText::FromString(TotalTimeText), FCoreStyle::GetDefaultFontStyle("Normal", 9),
        ESlateDrawEffect::None, FLinearColor(0.4f, 0.4f, 0.4f, 1.0f));
}
