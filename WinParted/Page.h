#pragma once
#include <PantherConsole.h>

class Page
{
public:
	Page();
	void Initialize(Console* con);
	void Draw();
	void Update();
	void Run();
	void SetStatusText(const wchar_t* txt);
private:
	const wchar_t* text;
	const wchar_t* statusText;
	Console* console;
	bool drawHeader;
	bool drawStatus;
	bool drawClear;

	friend class PartitionManager;
protected:
	void SetDecorations(bool header, bool status, bool clear);
	void SetText(const wchar_t* txt);
	virtual void InitPage();
	virtual void DrawPage();
	virtual void UpdatePage();
	virtual void RunPage();
	Console* GetConsole();
};

