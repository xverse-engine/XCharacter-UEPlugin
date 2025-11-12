// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MotionGen/MoGenSetting.h"
#include "MotionGen/MoGenStruct.h"
#include "MotionGen/BVHImporter.h"
#include "MotionGen/BVHImportFactory.h"
#include "MotionGen/BVHImportSettings.h"
#include "XVCPluginSettings.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Framework/Application/SlateApplication.h"

/**
 * MotionGen服务类，负责处理动作生成相关的网络请求和任务管理
 */
class CHARACTERGEN_API FMotionGenService
{
public:
    FMotionGenService();
    ~FMotionGenService();

    /**
     * 获取远程BVH文件
     * @param OuterPath 输出路径
     * @param SourcePath 源文件URL
     * @param MoGenSetting MoGen设置
     * @param Task 任务对象
     * @param HandlePtr 进度通知句柄
     */
    void GetRemoteBVHFile(FString OuterPath, FString SourcePath, UMoGenSetting* MoGenSetting,
                          TSharedPtr<MoGenTask> Task, TSharedPtr<FProgressNotificationHandle> HandlePtr);

    /**
     * 认证请求
     * @param MoGenSetting MoGen设置
     * @return 是否成功发起认证请求
     */
    bool Authenticate(UMoGenSetting* MoGenSetting);

    /**
     * 创建任务
     * @param MoGenSetting MoGen设置
     */
    void CreateTask(UMoGenSetting* MoGenSetting);

    /**
     * 获取任务信息
     * @param Task 任务对象
     * @param MoGenSetting MoGen设置
     * @param TimerHandle 定时器句柄
     * @param HandlePtr 进度通知句柄
     */
    void GetTaskInfo(TSharedPtr<MoGenTask> Task, UMoGenSetting* MoGenSetting,
                     TSharedPtr<FTimerHandle> TimerHandle, TSharedPtr<FProgressNotificationHandle> HandlePtr);

    /**
     * 心跳检测任务状态
     * @param Task 任务对象
     * @param MoGenSetting MoGen设置
     */
    void TickInfo(TSharedPtr<MoGenTask> Task, UMoGenSetting* MoGenSetting);

private:
    // BVH导入器
    FBVHImporter* Importer;
    
    // BVH导入工厂
    UBVHImportFactory* BVHFactory;
}; 