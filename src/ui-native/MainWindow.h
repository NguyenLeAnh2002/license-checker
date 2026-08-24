#pragma once

#include <windows.h>
#include <string>
#include <memory>

class PipeClient;

class MainWindow {
public:
    MainWindow();
    ~MainWindow();

    bool Create();
    void Show(int nCmdShow);
    HWND GetHandle() const { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK TabProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void CreateControls();
    void OnSize(int cx, int cy);
    void OnCommand(WPARAM wParam, LPARAM lParam);
    void OnNotify(LPARAM lParam);
    void OnLanguageChanged();
    void SwitchTab(int tabIndex);
    void UpdateChromeTexts();
    void UpdateContentLabelsPrefix();
    void UpdateUITexts();
    void RefreshLicenseData();
    void CheckNow();

    HWND m_hwnd;
    HWND m_hTabControl;
    HWND m_hCheckButton;
    HWND m_hRefreshLabel;
    HWND m_hLanguageLabel;
    HWND m_hLanguageCombo;

    // Tab 1: Windows License
    HWND m_hWindowsStatusLabel;
    HWND m_hWindowsNameLabel;
    HWND m_hWindowsEditionLabel;
    HWND m_hWindowsDescriptionLabel;
    HWND m_hWindowsLicenseDetailLabel;
    HWND m_hWindowsProductKeyLabel;
    HWND m_hWindowsKmsStatusLabel;
    HWND m_hWindowsKmsLabel;
    HWND m_hWindowsLastDetectedLabel;

    // Tab 2: Office License
    HWND m_hOfficeStatusLabel;
    HWND m_hOfficeLicenseNameLabel;
    HWND m_hOfficeLicenseStatusLabel;
    HWND m_hOfficeProductKeyLabel;
    HWND m_hOfficeKmsLabel;
    HWND m_hOfficeGracePeriodLabel;
    HWND m_hOfficeActivationIntervalLabel;

    // Tab 3: Settings
    HWND m_hHostnameLabel;
    HWND m_hMachineGuidLabel;
    HWND m_hDepartmentLabel;
    HWND m_hServiceStatusLabel;
    HWND m_hLastReportLabel;

    std::string m_lastWindowsStatus;
    std::string m_lastWindowsEdition;
    std::string m_lastWindowsKms;

    std::unique_ptr<PipeClient> m_pipeClient;
};
