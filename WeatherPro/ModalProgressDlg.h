#pragma once
#include "afxdialogex.h"

#include <functional>

// 进度模式枚举
enum class ProgressMode : std::uint8_t
{
	Percent,    // 百分比进度模式
	Marquee     // 无进度（跑马灯）模式
};

using SignalProgressPercent = std::function<void(int)>;

using TaskInMarqueeMode = std::function<void()>;
using TaskInPercentMode = std::function<void(const SignalProgressPercent&)>;

// ModalProgressDlg 对话框

class ModalTaskProgressDlg : public CDialogEx
{
	DECLARE_DYNAMIC(ModalTaskProgressDlg)

public:
	ModalTaskProgressDlg(CWnd* pParent, TaskInMarqueeMode &&task);
	ModalTaskProgressDlg(CWnd* pParent, TaskInPercentMode &&task);
	virtual ~ModalTaskProgressDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DLG_PROGRESS };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

	DECLARE_MESSAGE_MAP()

    virtual BOOL OnInitDialog();

	CProgressCtrl ctrl_progress;
	ProgressMode current_mode;

	TaskInMarqueeMode task_marquee;
	TaskInPercentMode task_percent;

	void StartTask();
	void RunTaskMarquee();
	void RunTaskPercent();

	afx_msg LRESULT OnStartTask(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnCompleteTask(WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnUpdateProgPercent(WPARAM wParam, LPARAM lParam);

	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	virtual BOOL PreTranslateMessage(MSG* pMsg);
public:
	CString title;
};
