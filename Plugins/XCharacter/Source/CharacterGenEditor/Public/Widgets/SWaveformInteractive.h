// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Rendering/DrawElements.h"
#include "Input/Reply.h"
#include "Layout/Geometry.h"
#include "Input/Events.h"

// 前向声明
struct FAudioSegmentInfo;

// 片段点击委托
DECLARE_DELEGATE_OneParam(FOnSegmentClicked, int32 /* SegmentIndex */);

// 片段悬停委托
DECLARE_DELEGATE_TwoParams(FOnSegmentHovered, int32 /* SegmentIndex */, bool /* bIsHovered */);

/**
 * 交互式波形绘制控件
 * 支持每个片段单独绘制和交互，可以点击片段进行播放等操作
 */
class CHARACTERGENEDITOR_API SWaveformInteractive : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SWaveformInteractive) 
        : _Samples(nullptr)
        , _NumSamples(0)
        , _SampleRate(0) 
        , _PlayProgress(0.0f)
        , _SelectedSegmentIndex(-1)
        , _HoveredSegmentIndex(-1)
    {}
        SLATE_ARGUMENT(const int16*, Samples)
        SLATE_ARGUMENT(int32, NumSamples)
        SLATE_ARGUMENT(uint32, SampleRate)
        SLATE_ATTRIBUTE(TArray<FAudioSegmentInfo>, Segments)
        SLATE_ATTRIBUTE(float, PlayProgress)
        SLATE_ATTRIBUTE(int32, SelectedSegmentIndex)
        SLATE_ATTRIBUTE(int32, HoveredSegmentIndex)
        SLATE_EVENT(FOnSegmentClicked, OnSegmentClicked)
        SLATE_EVENT(FOnSegmentHovered, OnSegmentHovered)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
                         const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, 
                         int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

    virtual FVector2D ComputeDesiredSize(float) const override;

    // 鼠标事件处理
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

private:
    /**
     * 绘制单个片段的波形
     * @param OutDrawElements 绘制元素列表
     * @param LayerId 绘制层级
     * @param AllottedGeometry 分配几何体
     * @param Size 控件大小
     * @param SegmentIndex 片段索引
     * @param Segment 片段信息
     * @param PixelsPerSecond 每秒像素数
     * @param WaveformHeight 波形高度
     */
    void DrawSegmentWaveform(FSlateWindowElementList& OutDrawElements, int32 LayerId, 
                            const FGeometry& AllottedGeometry, const FVector2D& Size,
                            int32 SegmentIndex, const FAudioSegmentInfo& Segment,
                            float PixelsPerSecond, float WaveformHeight) const;

    /**
     * 绘制片段标记和标签
     * @param OutDrawElements 绘制元素列表
     * @param LayerId 绘制层级
     * @param AllottedGeometry 分配几何体
     * @param Size 控件大小
     * @param SegmentIndex 片段索引
     * @param Segment 片段信息
     * @param PixelsPerSecond 每秒像素数
     * @param WaveformHeight 波形高度
     */
    void DrawSegmentMarker(FSlateWindowElementList& OutDrawElements, int32 LayerId, 
                          const FGeometry& AllottedGeometry, const FVector2D& Size,
                          int32 SegmentIndex, const FAudioSegmentInfo& Segment,
                          float PixelsPerSecond, float WaveformHeight) const;

    /**
     * 绘制时间刻度线
     * @param OutDrawElements 绘制元素列表
     * @param LayerId 绘制层级
     * @param AllottedGeometry 分配几何体
     * @param Size 控件大小
     */
    void DrawTimeScale(FSlateWindowElementList& OutDrawElements, int32 LayerId, 
                      const FGeometry& AllottedGeometry, const FVector2D& Size) const;

    /**
     * 根据鼠标位置获取对应的片段索引
     * @param MousePosition 鼠标位置
     * @param Size 控件大小
     * @return 片段索引，-1表示没有命中任何片段
     */
    int32 GetSegmentIndexAtPosition(const FVector2D& MousePosition, const FVector2D& Size) const;

    /**
     * 获取片段的屏幕区域
     * @param Segment 片段信息
     * @param PixelsPerSecond 每秒像素数
     * @param Size 控件大小
     * @return 片段的屏幕区域
     */
    FSlateRect GetSegmentScreenRect(const FAudioSegmentInfo& Segment, float PixelsPerSecond, const FVector2D& Size) const;

    /**
     * 计算鼠标位置对应的时间
     * @param MousePosition 鼠标位置
     * @param Size 控件大小
     * @return 对应的时间（秒）
     */
    float GetTimeAtMousePosition(const FVector2D& MousePosition, const FVector2D& Size) const;

    /**
     * 计算时间对应的屏幕位置
     * @param Time 时间（秒）
     * @param PixelsPerSecond 每秒像素数
     * @return 屏幕位置
     */
    float GetScreenPositionAtTime(float Time, float PixelsPerSecond) const;

    /**
     * 以指定时间为中心进行缩放
     * @param NewZoomLevel 新的缩放级别
     * @param CenterTime 缩放中心时间
     * @param Size 控件大小
     */
    void ZoomToTime(float NewZoomLevel, float CenterTime, const FVector2D& Size);

    /**
     * 获取当前缩放状态
     * @param OutZoomLevel 输出的缩放级别
     * @param OutPanOffset 输出的平移偏移
     */
    void GetZoomState(float& OutZoomLevel, float& OutPanOffset) const;

    /**
     * 设置缩放状态
     * @param ZoomLevel 缩放级别
     * @param PanOffset 平移偏移
     */
    void SetZoomState(float ZoomLevel, float PanOffset);

    /**
     * 计算动态最小缩放级别
     * @param Size 控件大小
     * @return 动态计算的最小缩放级别
     */
    float CalculateDynamicMinZoomLevel(const FVector2D& Size) const;

private:
    const int16* Samples = nullptr;
    int32 NumSamples = 0;
    uint32 SampleRate = 0;
    TAttribute<TArray<FAudioSegmentInfo>> Segments;
    TAttribute<float> PlayProgress;
    TAttribute<int32> SelectedSegmentIndex;
    TAttribute<int32> HoveredSegmentIndex;
    
    // 委托事件
    FOnSegmentClicked OnSegmentClicked;
    FOnSegmentHovered OnSegmentHovered;
    
    // 鼠标状态
    mutable int32 CachedHoveredSegment = -1;
    mutable bool bIsMouseOverWidget = false;
    
    // 缩放和平移状态
    mutable float ZoomLevel = 1.0f;           // 缩放级别 (1.0 = 100%)
    mutable float PanOffset = 0.0f;           // 平移偏移量（像素）
    mutable bool bIsPanning = false;          // 是否正在拖拽平移
    mutable FVector2D LastPanMousePosition;   // 上次拖拽的鼠标位置
    
    // 缩放中心点相关
    mutable float ZoomCenterTime = 0.0f;      // 缩放中心点的时间位置（秒）
    mutable bool bUseMouseCenterZoom = true;  // 是否使用鼠标中心缩放
    
    // 缩放和平移限制
    static constexpr float MinZoomLevel = 0.1f;   // 最小缩放级别
    static constexpr float MaxZoomLevel = 10.0f;  // 最大缩放级别
    static constexpr float ZoomStep = 0.1f;       // 缩放步长
};
