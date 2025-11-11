// Copyright Epic Games, Inc. All Rights Reserved.

#include "MotionGen/BVHImportOptions.h"

#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "MotionGen/BVHImportSettings.h"
#include "Math/UnrealMathUtility.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "BVHImportOptions"

void SBVHImportOptions::Construct(const FArguments& InArgs)
{
	ImportSettings = InArgs._ImportSettings;
	WidgetWindow = InArgs._WidgetWindow;

	check(ImportSettings);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.ColumnWidth = 0.5f;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(ImportSettings);

	static const float MaxDesiredHeight = 250.f;

	this->ChildSlot
		[
			SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(2)
				.MaxHeight(500.0f)
				[
					DetailsView->AsShared()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(2)
				[
					SNew(SUniformGridPanel)
						.SlotPadding(2)
						+ SUniformGridPanel::Slot(0, 0)
						[
							SAssignNew(ImportButton, SButton)
								.HAlign(HAlign_Center)
								.Text(LOCTEXT("BVHOptionWindow_Import", "Import"))
								.IsEnabled(this, &SBVHImportOptions::CanImport)
								.OnClicked(this, &SBVHImportOptions::OnImport)
						]
					+ SUniformGridPanel::Slot(1, 0)
						[
							SNew(SButton)
								.HAlign(HAlign_Center)
								.Text(LOCTEXT("BVHOptionWindow_Cancel", "Cancel"))
								.ToolTipText(LOCTEXT("BVHOptionWindow_Cancel_ToolTip", "Cancels importing this BVH file"))
								.OnClicked(this, &SBVHImportOptions::OnCancel)
						]
				]
		];
}

bool SBVHImportOptions::CanImport()  const
{
	return true;
}

void SBVHImportOptions::OnToggleAllItems(ECheckBoxState CheckType)
{
}

#undef LOCTEXT_NAMESPACE

