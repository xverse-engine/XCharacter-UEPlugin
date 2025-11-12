// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SWaveformInteractive.h"
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
#include "TTSPreviewManager.h"

void SWaveformInteractive::Construct(const FArguments& InArgs)
{
    Samples = InArgs._Samples;
    NumSamples = InArgs._NumSamples;
    SampleRate = InArgs._SampleRate;
    Segments = InArgs._Segments;
    PlayProgress = InArgs._PlayProgress;
    SelectedSegmentIndex = InArgs._SelectedSegmentIndex;
    HoveredSegmentIndex = InArgs._HoveredSegmentIndex;
    OnSegmentClicked = InArgs._OnSegmentClicked;
    OnSegmentHovered = InArgs._OnSegmentHovered;
}

int32 SWaveformInteractive::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
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

    // 计算每秒像素数（应用缩放和平移）
    const float TotalDuration = (float)NumSamples / (float)SampleRate;
    const float BasePixelsPerSecond = Size.X / TotalDuration;
    const float PixelsPerSecond = BasePixelsPerSecond * ZoomLevel;

    // 绘制每个片段的波形
    if (Segments.IsSet() && Segments.Get().Num() > 0)
    {
        const TArray<FAudioSegmentInfo>& SegmentsArray = Segments.Get();
        
        // 限制绘制的片段数量，避免性能问题
        const int32 MaxSegmentsToDraw = 50;
        const int32 SegmentsToDraw = FMath::Min(SegmentsArray.Num(), MaxSegmentsToDraw);
        
        for (int32 i = 0; i < SegmentsToDraw; ++i)
        {
            const FAudioSegmentInfo& Segment = SegmentsArray[i];
            
            // 验证Segment数据的有效性
            if (FMath::IsNaN(Segment.StartTime) || FMath::IsNaN(Segment.EndTime) || 
                Segment.StartTime < 0.0f || Segment.EndTime < Segment.StartTime)
            {
                continue;
            }
            
            // 绘制片段波形
            DrawSegmentWaveform(OutDrawElements, LayerId, AllottedGeometry, Size, i, Segment, PixelsPerSecond, WaveformHeight);
            LayerId++;
            
            // 绘制片段标记
            DrawSegmentMarker(OutDrawElements, LayerId, AllottedGeometry, Size, i, Segment, PixelsPerSecond, WaveformHeight);
            LayerId++;
        }
    }
    else
    {
        // 如果没有片段信息，绘制整个波形
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
    }

    // 绘制播放进度指示器
    if (PlayProgress.IsSet())
    {
        const float Progress = FMath::Clamp(PlayProgress.Get(), 0.0f, 1.0f);
        
        // 计算进度条位置，考虑缩放和平移状态
        const float ProgressTime = Progress * TotalDuration;  // 当前播放时间
        const float ProgressX = ProgressTime * PixelsPerSecond + PanOffset;  // 应用缩放和平移
        
        // 检查进度条是否在可见区域内
        if (ProgressX >= 0.0f && ProgressX <= Size.X)
        {
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
    }

    // 绘制时间刻度线
    if (SampleRate > 0 && NumSamples > 0)
    {
        DrawTimeScale(OutDrawElements, LayerId, AllottedGeometry, Size);
        LayerId++;
    }

    return LayerId;
}

FVector2D SWaveformInteractive::ComputeDesiredSize(float) const
{
    return FVector2D(800.f, 140.f); // 增加20像素高度用于时间刻度线
}

FReply SWaveformInteractive::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    const FVector2D MousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    const FVector2D Size = MyGeometry.GetLocalSize();
    
    if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        int32 ClickedSegmentIndex = GetSegmentIndexAtPosition(MousePosition, Size);
        if (ClickedSegmentIndex >= 0)
        {
            // 触发片段点击事件
            OnSegmentClicked.ExecuteIfBound(ClickedSegmentIndex);
            return FReply::Handled();
        }
    }
    else if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        // 开始右键拖拽平移
        bIsPanning = true;
        LastPanMousePosition = MousePosition;
        return FReply::Handled();
    }
    
    return FReply::Unhandled();
}

FReply SWaveformInteractive::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        // 停止右键拖拽平移
        bIsPanning = false;
        return FReply::Handled();
    }
    
    return FReply::Unhandled();
}

FReply SWaveformInteractive::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    const FVector2D MousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    const FVector2D Size = MyGeometry.GetLocalSize();
    
    // 处理右键拖拽平移
    if (bIsPanning)
    {
        const float DeltaX = MousePosition.X - LastPanMousePosition.X;
        PanOffset += DeltaX;
        
        // 移除严格的边界限制，允许自由拖拽
        // 只在绘制时进行边界检查，避免拖拽时的闪烁
        
        LastPanMousePosition = MousePosition;
        return FReply::Handled();
    }
    
    int32 NewHoveredSegment = GetSegmentIndexAtPosition(MousePosition, Size);
    
    // 检查悬停状态是否改变
    if (NewHoveredSegment != CachedHoveredSegment)
    {
        // 取消之前的悬停
        if (CachedHoveredSegment >= 0)
        {
            OnSegmentHovered.ExecuteIfBound(CachedHoveredSegment, false);
        }
        
        // 设置新的悬停
        CachedHoveredSegment = NewHoveredSegment;
        if (CachedHoveredSegment >= 0)
        {
            OnSegmentHovered.ExecuteIfBound(CachedHoveredSegment, true);
        }
    }
    
    bIsMouseOverWidget = true;
    return FReply::Unhandled();
}

FReply SWaveformInteractive::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    // 处理鼠标滚轮缩放
    const float Delta = MouseEvent.GetWheelDelta();
    const float OldZoomLevel = ZoomLevel;
    const FVector2D Size = MyGeometry.GetLocalSize();
    
    // 计算动态最小缩放级别
    const float DynamicMinZoomLevel = CalculateDynamicMinZoomLevel(Size);
    
    // 计算新的缩放级别
    const float NewZoomLevel = FMath::Clamp(ZoomLevel + Delta * ZoomStep, DynamicMinZoomLevel, MaxZoomLevel);
    
    // 如果缩放级别发生变化，执行以鼠标为中心的缩放
    if (NewZoomLevel != OldZoomLevel)
    {
        if (bUseMouseCenterZoom)
        {
            // 获取鼠标位置对应的时间作为缩放中心
            const FVector2D MousePosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
            const float CenterTime = GetTimeAtMousePosition(MousePosition, Size);
            
            // 以鼠标位置为中心进行缩放
            ZoomToTime(NewZoomLevel, CenterTime, Size);
        }
        else
        {
            // 简单的缩放（不使用鼠标中心）
            ZoomLevel = NewZoomLevel;
        }
        
        return FReply::Handled();
    }
    
    return FReply::Unhandled();
}

void SWaveformInteractive::OnMouseLeave(const FPointerEvent& MouseEvent)
{
    // 取消悬停状态
    if (CachedHoveredSegment >= 0)
    {
        OnSegmentHovered.ExecuteIfBound(CachedHoveredSegment, false);
        CachedHoveredSegment = -1;
    }
    
    // 停止拖拽
    bIsPanning = false;
    bIsMouseOverWidget = false;
}

void SWaveformInteractive::DrawSegmentWaveform(FSlateWindowElementList& OutDrawElements, int32 LayerId, 
                                              const FGeometry& AllottedGeometry, const FVector2D& Size,
                                              int32 SegmentIndex, const FAudioSegmentInfo& Segment,
                                              float PixelsPerSecond, float WaveformHeight) const
{
    // 计算片段在屏幕上的位置（应用平移偏移）
    float StartX = Segment.StartTime * PixelsPerSecond + PanOffset;
    float EndX = Segment.EndTime * PixelsPerSecond + PanOffset;
    
    // 限制位置范围，防止绘制超出边界
    StartX = FMath::Clamp(StartX, 0.0f, Size.X);
    EndX = FMath::Clamp(EndX, 0.0f, Size.X);
    
    if (StartX >= Size.X || EndX <= 0.0f)
    {
        return; // 片段不在可见区域内
    }
    
    // 计算片段对应的样本范围
    int32 StartSample = FMath::RoundToInt(Segment.StartTime * SampleRate);
    int32 EndSample = FMath::RoundToInt(Segment.EndTime * SampleRate);
    StartSample = FMath::Clamp(StartSample, 0, NumSamples - 1);
    EndSample = FMath::Clamp(EndSample, 0, NumSamples - 1);
    
    if (StartSample >= EndSample)
    {
        return; // 无效的样本范围
    }
    
    // 绘制片段的波形
    TArray<FVector2D> Points;
    const int32 SegmentSampleCount = EndSample - StartSample;
    const int32 Step = FMath::Max(1, SegmentSampleCount / FMath::Max<int32>(1, (int32)(EndX - StartX)));
    
    for (int32 i = StartSample; i < EndSample; i += Step)
    {
        const float Norm = (float)Samples[i] / 32768.f;
        const float y = (WaveformHeight * 0.5f) - Norm * (WaveformHeight * 0.5f);
        const float x = StartX + ((float)(i - StartSample) / (float)SegmentSampleCount) * (EndX - StartX);
        Points.Add(FVector2D(x, y));
    }
    
    // 确定绘制颜色
    FLinearColor WaveformColor = Segment.Color;
    
    // 根据状态调整颜色
    if (SelectedSegmentIndex.IsSet() && SelectedSegmentIndex.Get() == SegmentIndex)
    {
        // 选中状态：更亮的颜色
        WaveformColor = WaveformColor * 1.3f;
        WaveformColor.A = 1.0f;
    }
    else if (HoveredSegmentIndex.IsSet() && HoveredSegmentIndex.Get() == SegmentIndex)
    {
        // 悬停状态：稍微亮一点
        WaveformColor = WaveformColor * 1.1f;
        WaveformColor.A = 1.0f;
    }
    else if (Segment.bIsSilence)
    {
        // 静音区域：灰色
        WaveformColor = FLinearColor(0.6f, 0.6f, 0.6f, 0.8f);
    }
    
    if (Points.Num() > 1)
    {
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), 
                                    Points, ESlateDrawEffect::None, WaveformColor, true, 1.5f);
    }
}

void SWaveformInteractive::DrawSegmentMarker(FSlateWindowElementList& OutDrawElements, int32 LayerId, 
                                            const FGeometry& AllottedGeometry, const FVector2D& Size,
                                            int32 SegmentIndex, const FAudioSegmentInfo& Segment,
                                            float PixelsPerSecond, float WaveformHeight) const
{
    // 计算片段在屏幕上的位置（应用平移偏移）
    float StartX = Segment.StartTime * PixelsPerSecond + PanOffset;
    float EndX = Segment.EndTime * PixelsPerSecond + PanOffset;
    
    // 限制位置范围，防止绘制超出边界
    StartX = FMath::Clamp(StartX, 0.0f, Size.X);
    EndX = FMath::Clamp(EndX, 0.0f, Size.X);
    
    // 确定标记线颜色
    FLinearColor LineColor = Segment.bIsSilence ? 
        FLinearColor(0.8f, 0.8f, 0.8f, 0.7f) :  // 静音区域用灰色
        Segment.Color;
    
    // 根据状态调整颜色
    if (SelectedSegmentIndex.IsSet() && SelectedSegmentIndex.Get() == SegmentIndex)
    {
        LineColor = LineColor * 1.3f;
        LineColor.A = 1.0f;
    }
    else if (HoveredSegmentIndex.IsSet() && HoveredSegmentIndex.Get() == SegmentIndex)
    {
        LineColor = LineColor * 1.1f;
        LineColor.A = 1.0f;
    }
    
    // 绘制开始标记线
    if (StartX >= 0 && StartX <= Size.X)
    {
        TArray<FVector2D> StartLine;
        StartLine.Add(FVector2D(StartX, 0));
        StartLine.Add(FVector2D(StartX, WaveformHeight));
        
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), 
            StartLine, ESlateDrawEffect::None, LineColor, true, 2.0f);
    }
    
    // 绘制结束标记线
    if (EndX >= 0 && EndX <= Size.X && EndX != StartX)
    {
        TArray<FVector2D> EndLine;
        EndLine.Add(FVector2D(EndX, 0));
        EndLine.Add(FVector2D(EndX, WaveformHeight));
        
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), 
            EndLine, ESlateDrawEffect::None, LineColor, true, 2.0f);
    }
    
    // 绘制标签文本
    if (!Segment.Label.IsEmpty())
    {
        // 限制文本长度，避免显示问题
        FString ShortLabel = Segment.Label.Len() > 30 ? Segment.Label.Left(30) + TEXT("...") : Segment.Label;
        
        // 计算文本位置
        FVector2D TextPosition;
        float EstimatedTextWidth = ShortLabel.Len() * 7.0f; // 平均每个字符7像素
        EstimatedTextWidth = FMath::Min(EstimatedTextWidth, 120.0f);
        
        if (Segment.bIsSilence)
        {
            // 静音区域：文本居中显示
            float SegmentWidth = FMath::Max(1.0f, EndX - StartX);
            if (SegmentWidth >= EstimatedTextWidth + 10)
            {
                float CenterX = StartX + SegmentWidth * 0.5f;
                TextPosition = FVector2D(CenterX - EstimatedTextWidth * 0.5f, 2);
            }
            else
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
        
        // 绘制文本
        FVector2D TextSize = FVector2D(EstimatedTextWidth, 16.0f);
        if (TextSize.X > 0 && TextSize.Y > 0)
        {
            FSlateDrawElement::MakeText(OutDrawElements, LayerId, 
                AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextPosition)),
                FText::FromString(ShortLabel), FCoreStyle::GetDefaultFontStyle("Normal", 8),
                ESlateDrawEffect::None, LineColor);
        }
    }
    
    // 如果是静音区域，绘制背景填充
    if (Segment.bIsSilence && EndX > StartX)
    {
        float SilenceWidth = EndX - StartX;
        if (SilenceWidth > 0 && FMath::IsFinite(SilenceWidth))
        {
            FVector2D SilenceSize = FVector2D(SilenceWidth, WaveformHeight);
            FVector2D SilencePosition = FVector2D(StartX, 0);
            
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

void SWaveformInteractive::DrawTimeScale(FSlateWindowElementList& OutDrawElements, int32 LayerId, 
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

    // 根据缩放级别和总时长计算时间刻度的间隔
    float TimeInterval = 1.0f; // 默认间隔
    
    // 根据总时长选择基础间隔
    if (TotalDuration <= 5.0f)
    {
        TimeInterval = 1.0f; // 5秒以内用1秒间隔
    }
    else if (TotalDuration <= 15.0f)
    {
        TimeInterval = 2.0f; // 15秒以内用2秒间隔
    }
    else if (TotalDuration <= 30.0f)
    {
        TimeInterval = 5.0f; // 30秒以内用5秒间隔
    }
    else if (TotalDuration <= 120.0f)
    {
        TimeInterval = 10.0f; // 2分钟以内用10秒间隔
    }
    else if (TotalDuration <= 300.0f)
    {
        TimeInterval = 20.0f; // 5分钟以内用20秒间隔
    }
    else
    {
        TimeInterval = 30.0f; // 更长用30秒间隔
    }
    
    // 根据缩放级别调整间隔，让刻度更稀疏
    if (ZoomLevel >= 3.0f)
    {
        TimeInterval = TimeInterval * 0.5f; // 高缩放时稍微密集一些
    }
    else if (ZoomLevel >= 1.5f)
    {
        TimeInterval = TimeInterval * 0.7f; // 中等缩放
    }
    else if (ZoomLevel >= 1.0f)
    {
        TimeInterval = TimeInterval; // 正常缩放，使用基础间隔
    }
    else
    {
        TimeInterval = TimeInterval * 1.5f; // 低缩放时更稀疏
    }
    
    // 确保间隔不会太小或太大
    TimeInterval = FMath::Clamp(TimeInterval, 0.5f, 60.0f);

    const float BasePixelsPerSecond = Size.X / TotalDuration;
    const float PixelsPerSecond = BasePixelsPerSecond * ZoomLevel;
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
        const float X = Time * PixelsPerSecond + PanOffset;
        
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

int32 SWaveformInteractive::GetSegmentIndexAtPosition(const FVector2D& MousePosition, const FVector2D& Size) const
{
    if (!Segments.IsSet() || Segments.Get().Num() == 0)
    {
        return -1;
    }
    
    const TArray<FAudioSegmentInfo>& SegmentsArray = Segments.Get();
    const float TotalDuration = (float)NumSamples / (float)SampleRate;
    const float BasePixelsPerSecond = Size.X / TotalDuration;
    const float PixelsPerSecond = BasePixelsPerSecond * ZoomLevel;
    
    // 从后往前检查，这样后面的片段会优先被选中（覆盖前面的）
    for (int32 i = SegmentsArray.Num() - 1; i >= 0; --i)
    {
        const FAudioSegmentInfo& Segment = SegmentsArray[i];
        
        // 验证Segment数据的有效性
        if (FMath::IsNaN(Segment.StartTime) || FMath::IsNaN(Segment.EndTime) || 
            Segment.StartTime < 0.0f || Segment.EndTime < Segment.StartTime)
        {
            continue;
        }
        
        FSlateRect SegmentRect = GetSegmentScreenRect(Segment, PixelsPerSecond, Size);
        
        // 检查鼠标位置是否在片段区域内
        if (SegmentRect.ContainsPoint(MousePosition))
        {
            return i;
        }
    }
    
    return -1;
}

FSlateRect SWaveformInteractive::GetSegmentScreenRect(const FAudioSegmentInfo& Segment, float PixelsPerSecond, const FVector2D& Size) const
{
    float StartX = Segment.StartTime * PixelsPerSecond + PanOffset;
    float EndX = Segment.EndTime * PixelsPerSecond + PanOffset;
    
    // 限制位置范围
    StartX = FMath::Clamp(StartX, 0.0f, Size.X);
    EndX = FMath::Clamp(EndX, 0.0f, Size.X);
    
    const float TimeScaleHeight = 20.0f;
    const float WaveformHeight = Size.Y - TimeScaleHeight;
    
    return FSlateRect(StartX, 0.0f, EndX, WaveformHeight);
}

float SWaveformInteractive::GetTimeAtMousePosition(const FVector2D& MousePosition, const FVector2D& Size) const
{
    if (SampleRate <= 0 || NumSamples <= 0)
    {
        return 0.0f;
    }
    
    const float TotalDuration = (float)NumSamples / (float)SampleRate;
    const float BasePixelsPerSecond = Size.X / TotalDuration;
    const float PixelsPerSecond = BasePixelsPerSecond * ZoomLevel;
    
    // 将鼠标位置转换为时间
    // 考虑平移偏移：Time = (MouseX - PanOffset) / PixelsPerSecond
    const float Time = (MousePosition.X - PanOffset) / PixelsPerSecond;
    
    // 限制时间范围
    return FMath::Clamp(Time, 0.0f, TotalDuration);
}

float SWaveformInteractive::GetScreenPositionAtTime(float Time, float PixelsPerSecond) const
{
    // 将时间转换为屏幕位置
    // 考虑平移偏移：ScreenX = Time * PixelsPerSecond + PanOffset
    return Time * PixelsPerSecond + PanOffset;
}

void SWaveformInteractive::ZoomToTime(float NewZoomLevel, float CenterTime, const FVector2D& Size)
{
    if (SampleRate <= 0 || NumSamples <= 0)
    {
        return;
    }
    
    const float TotalDuration = (float)NumSamples / (float)SampleRate;
    const float OldZoomLevel = ZoomLevel;
    
    // 计算缩放前后的像素比例
    const float BasePixelsPerSecond = Size.X / TotalDuration;
    const float OldPixelsPerSecond = BasePixelsPerSecond * OldZoomLevel;
    const float NewPixelsPerSecond = BasePixelsPerSecond * NewZoomLevel;
    
    // 计算缩放中心点在屏幕上的位置（使用旧的缩放级别，但不包含PanOffset）
    const float CenterScreenPosition = CenterTime * OldPixelsPerSecond + PanOffset;
    
    // 计算缩放后中心点应该在的位置（使用新的缩放级别，但不包含PanOffset）
    const float NewCenterScreenPosition = CenterTime * NewPixelsPerSecond + PanOffset;
    
    // 调整平移偏移，使缩放中心点保持在鼠标位置
    const float DeltaOffset = CenterScreenPosition - NewCenterScreenPosition;
    PanOffset += DeltaOffset;
    
    // 更新缩放级别
    ZoomLevel = NewZoomLevel;
    
    // 移除严格的边界限制，允许中心缩放正常工作
    // 只在绘制时进行边界检查，避免缩放时的位置偏移
}

void SWaveformInteractive::GetZoomState(float& OutZoomLevel, float& OutPanOffset) const
{
    OutZoomLevel = ZoomLevel;
    OutPanOffset = PanOffset;
}

void SWaveformInteractive::SetZoomState(float InZoomLevel, float InPanOffset)
{
    // 注意：这里使用默认的最小缩放级别，因为SetZoomState通常用于恢复状态
    // 如果需要动态限制，应该在调用时传入正确的Size参数
    ZoomLevel = FMath::Clamp(InZoomLevel, MinZoomLevel, MaxZoomLevel);
    PanOffset = InPanOffset;
}

float SWaveformInteractive::CalculateDynamicMinZoomLevel(const FVector2D& Size) const
{
    if (SampleRate <= 0 || NumSamples <= 0)
    {
        return MinZoomLevel; // 返回默认最小缩放级别
    }
    
    const float TotalDuration = (float)NumSamples / (float)SampleRate;
    if (TotalDuration <= 0.0f)
    {
        return MinZoomLevel; // 返回默认最小缩放级别
    }
    
    // 计算动态最小缩放级别
    // 最小缩放级别应该确保整个音频内容能够完全填充控件宽度
    // 即：TotalDuration * PixelsPerSecond = Size.X
    // 其中：PixelsPerSecond = (Size.X / TotalDuration) * ZoomLevel
    // 所以：TotalDuration * (Size.X / TotalDuration) * ZoomLevel = Size.X
    // 简化：Size.X * ZoomLevel = Size.X
    // 所以：ZoomLevel = 1.0f
    
    // 最小缩放级别应该是1.0f，确保波形完全填充控件
    const float DynamicMinZoom = 1.0f;
    
    // 确保动态最小缩放级别不小于默认最小缩放级别
    return FMath::Max(DynamicMinZoom, MinZoomLevel);
}
