// ModalProgressDlg.cpp: 实现文件
//

#include "pch.h"
#include "WeatherPro.h"
#include "afxdialogex.h"
#include "ModalProgressDlg.h"
#include "resource.h"

#include <chrono>
#include <thread>

// ModalProgressDlg 对话框

namespace
{
	constexpr int WM_MTPD_START_TASK{ WM_USER + 100 };
	constexpr int WM_MTPD_COMPLETE_TASK{ WM_USER + 101 };
	constexpr int WM_MTPD_UPDATE_PROG_PERCENT{ WM_USER + 102 };
}

IMPLEMENT_DYNAMIC(ModalTaskProgressDlg, CDialogEx)

ModalTaskProgressDlg::ModalTaskProgressDlg(CWnd* pParent, TaskInMarqueeMode &&task)
	: CDialogEx(IDD_DLG_PROGRESS, pParent) {
	current_mode = ProgressMode::Marquee;
	task_marquee = std::move(task);
}

ModalTaskProgressDlg::ModalTaskProgressDlg(CWnd *pParent, TaskInPercentMode &&task)
	: CDialogEx(IDD_DLG_PROGRESS, pParent) {
	current_mode = ProgressMode::Percent;
	task_percent = std::move(task);
}

ModalTaskProgressDlg::~ModalTaskProgressDlg()
{
}

void ModalTaskProgressDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PROGRESS_X, ctrl_progress);
}


BEGIN_MESSAGE_MAP(ModalTaskProgressDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
    ON_MESSAGE(WM_MTPD_START_TASK, OnStartTask)
	ON_MESSAGE(WM_MTPD_COMPLETE_TASK, OnCompleteTask)
    ON_MESSAGE(WM_MTPD_UPDATE_PROG_PERCENT, OnUpdateProgPercent)
END_MESSAGE_MAP()


// ModalProgressDlg 消息处理程序

BOOL ModalTaskProgressDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 移除关闭命令
	if (CMenu* pSysMenu = GetSystemMenu(FALSE);
		pSysMenu != nullptr) {
		VERIFY(pSysMenu->RemoveMenu(SC_CLOSE, MF_BYCOMMAND));
	}

	if (current_mode == ProgressMode::Marquee) {
		ctrl_progress.ModifyStyle(0, PBS_MARQUEE);
	}

	SetWindowTextW(title + L" ...");

	PostMessageW(WM_MTPD_START_TASK);

	return TRUE;  // return TRUE unless you set the focus to a control
	// 异常: OCX 属性页应返回 FALSE
}

void ModalTaskProgressDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == SC_CLOSE) {
        return; // 忽略关闭命令
    }

	CDialog::OnSysCommand(nID, lParam);
}

void ModalTaskProgressDlg::RunTaskMarquee(){
	using namespace std::chrono_literals;

	ctrl_progress.SetMarquee(TRUE, 30);

	if (task_marquee != nullptr) {
		task_marquee();
	}

	std::this_thread::sleep_for(1s);
	PostMessageW(WM_MTPD_COMPLETE_TASK);
}

void ModalTaskProgressDlg::RunTaskPercent() {
	using namespace std::chrono_literals;

	ctrl_progress.SetRange32(0, 100);

	auto signal_percent = [this](int p) {
		PostMessageW(WM_MTPD_UPDATE_PROG_PERCENT, 0, p);
	};

	if (task_percent != nullptr) {
		task_percent(signal_percent);
	}

	std::this_thread::sleep_for(1s);
	PostMessageW(WM_MTPD_COMPLETE_TASK);
}

void ModalTaskProgressDlg::StartTask() {
    if (current_mode == ProgressMode::Marquee) {
		std::thread t(&ModalTaskProgressDlg::RunTaskMarquee, this);
		t.detach();
    } else {
		std::thread t(&ModalTaskProgressDlg::RunTaskPercent, this);
		t.detach();
    }
}

LRESULT ModalTaskProgressDlg::OnStartTask(WPARAM wParam, LPARAM lParam) {
	StartTask();
	return 0;
}

LRESULT ModalTaskProgressDlg::OnCompleteTask(WPARAM wParam, LPARAM lParam) {
	EndDialog(IDOK);
	return 0;
}

LRESULT ModalTaskProgressDlg::OnUpdateProgPercent(WPARAM wParam, LPARAM lParam) {
	const auto p = std::max(0, std::min(100, static_cast<int>(lParam)));
	ctrl_progress.SetPos(p);
	return 0;
}

BOOL ModalTaskProgressDlg::PreTranslateMessage(MSG* pMsg)
{
	if (pMsg->message == WM_KEYDOWN) {
		// 屏蔽 ESC 键（VK_ESCAPE）
		if (pMsg->wParam == VK_ESCAPE) {
			return TRUE;
		}
	}

	return CDialogEx::PreTranslateMessage(pMsg);
}
