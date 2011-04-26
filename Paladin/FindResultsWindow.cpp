#include "FindResultsWindow.h"

#include <Alert.h>
#include <Font.h>
#include <stdio.h>
#include <StringView.h>

#include "DListView.h"
#include "DTextView.h"
#include "LaunchHelper.h"

enum
{
	M_FIND = 'find',
	M_REPLACE = 'repl',
	M_REPLACE_ALL = 'rpla',
	M_TOGGLE_REGEX = 'tgrx',
	M_TOGGLE_CASE_INSENSITIVE = 'tgci',
	M_TOGGLE_MATCH_WORD = 'tgmw',
	M_FIND_CHANGED = 'fnch',
	M_SET_PROJECT = 'stpj'
};

enum
{
	THREAD_FIND = 0,
	THREAD_REPLACE,
	THREAD_REPLACE_ALL
};

void
TokenizeToList(const char *string, BObjectList<BString> &stringList)
{
	if (!string)
		return;
	
	char *workstr = new char[strlen(string) + 1];
	strcpy(workstr, string);
	strtok(workstr, "\n");
	
	char *token = strtok(NULL,"\n");
	char *lasttoken = workstr;
	
	if (!token)
	{
		delete [] workstr;
		stringList.AddItem(new BString(string));
		return;
	}
	
	int32 length = 0;
	BString *newword = NULL;
	
	while (token)
	{
		length = token - lasttoken;
		
		newword = new BString(lasttoken, length + 1);
		lasttoken = token;
		stringList.AddItem(newword);

		token = strtok(NULL,"\n");
	}
	
	length = strlen(lasttoken);
	newword = new BString(lasttoken, length + 1);
	lasttoken = token;
	stringList.AddItem(newword);
		
	delete [] workstr;
}


FindResultsWindow::FindResultsWindow(void)
	:	DWindow(BRect(100,100,600,500), "Find Results"),
		fIsRegEx(false),
		fMatchCase(false),
		fMatchWord(false),
		fThreadID(-1),
		fThreadMode(0),
		fThreadQuitFlag(0),
		fFileList(20, true)
{
/***********************************************************************************
************************************************************************************
								TEST CODE START
************************************************************************************
***********************************************************************************/
	fWorkingDir = "/boot/home/projects/FindResults/TestFiles/";
	fFileList.AddItem(new BString("App.h"));
	fFileList.AddItem(new BString("EditView.h"));
	fFileList.AddItem(new BString("MainWindow.h"));

/***********************************************************************************
************************************************************************************
								TEST CODE END
************************************************************************************
***********************************************************************************/
	MakeCenteredOnShow(true);
	BView *top = GetBackgroundView();
	
	BRect r(Bounds());
	r.bottom = 20.0;
	fMenuBar = new BMenuBar(r, "menubar");
	top->AddChild(fMenuBar);
	
	fFindButton = new BButton(BRect(0,0,1,1), "findbutton", "Replace All",
								new BMessage(M_FIND), B_FOLLOW_TOP | B_FOLLOW_RIGHT);
	fFindButton->ResizeToPreferred();
	fFindButton->SetLabel("Find");
	fFindButton->MoveTo(Bounds().right - fFindButton->Bounds().Width() - 10.0, 30.0);
	fFindButton->SetEnabled(false);
	
	font_height fh;
	be_plain_font->GetHeight(&fh);
	float lineHeight = fh.ascent + fh.descent + fh.leading;
	
	r = fFindButton->Frame();
	r.left = 10.0;
	r.right = fFindButton->Frame().left - 10.0 - B_V_SCROLL_BAR_WIDTH;
	r.bottom = r.top + (lineHeight * 2.0) + 10.0 + B_H_SCROLL_BAR_HEIGHT;
	fFindBox = new DTextView(r, "findbox", B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP);
	
	BScrollView *scroll = fFindBox->MakeScrollView("findscroll", true, true);
	top->AddChild(scroll);
	
	top->AddChild(fFindButton);
	
	r.OffsetTo(10.0, fFindBox->Parent()->Frame().bottom + 10.0);
	fReplaceBox = new DTextView(r, "replacebox", B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP);
	
	scroll = fReplaceBox->MakeScrollView("replacescroll", true, true);
	top->AddChild(scroll);
	
	fReplaceButton = new BButton(fFindButton->Bounds(), "replacebutton", "Replace",
								new BMessage(M_REPLACE), B_FOLLOW_TOP | B_FOLLOW_RIGHT);
	fReplaceButton->MoveTo(scroll->Frame().right + 10.0, scroll->Frame().top);
	top->AddChild(fReplaceButton);
	fReplaceButton->SetEnabled(false);
	
	fReplaceAllButton = new BButton(fReplaceButton->Frame(), "replaceallbutton", "Replace All",
								new BMessage(M_REPLACE_ALL), B_FOLLOW_TOP | B_FOLLOW_RIGHT);
	fReplaceAllButton->MoveBy(0.0, fReplaceAllButton->Frame().Height() + 10.0);
	top->AddChild(fReplaceAllButton);
	fReplaceAllButton->SetEnabled(false);
	
	BStringView *resultLabel = new BStringView(BRect(0,0,1,1), "resultlabel", "Results:");
	resultLabel->ResizeToPreferred();
	resultLabel->MoveTo(10.0, scroll->Frame().bottom + 5.0);
	top->AddChild(resultLabel);
	
	r = Bounds().InsetByCopy(10.0, 10.0);
	r.top = resultLabel->Frame().bottom + 5.0;
	r.right -= B_V_SCROLL_BAR_WIDTH;
	r.bottom -= B_H_SCROLL_BAR_HEIGHT;
	fResultList = new DListView(r, "resultlist", B_MULTIPLE_SELECTION_LIST, B_FOLLOW_ALL);
	scroll = fResultList->MakeScrollView("resultscroll", true, true);
	top->AddChild(scroll);
	
	BMenu *menu = new BMenu("Search");
	menu->AddItem(new BMenuItem("Find", new BMessage(M_FIND), 'F', B_COMMAND_KEY));
	menu->AddSeparatorItem();
	menu->AddItem(new BMenuItem("Replace", new BMessage(M_REPLACE), 'R', B_COMMAND_KEY));
	menu->AddItem(new BMenuItem("Replace All", new BMessage(M_REPLACE_ALL), 'R',
								B_COMMAND_KEY | B_SHIFT_KEY));
	fMenuBar->AddItem(menu);
	
	menu = new BMenu("Options");
	menu->AddItem(new BMenuItem("Regular Expression", new BMessage(M_TOGGLE_REGEX)));
	menu->AddItem(new BMenuItem("Ignore Case", new BMessage(M_TOGGLE_CASE_INSENSITIVE)));
	menu->AddItem(new BMenuItem("Match Whole Word", new BMessage(M_TOGGLE_MATCH_WORD)));
	fMenuBar->AddItem(menu);
	
	BMenuItem *item = fMenuBar->FindItem("Ignore Case");
	if (fMatchCase)
		item->SetMarked(true);
	
	menu = new BMenu("Project");
	fMenuBar->AddItem(menu);
	
	EnableReplace(false);
	
	// The search terms box will tell us whenever it has been changed at every keypress
	fFindBox->SetMessage(new BMessage(M_FIND_CHANGED));
	fFindBox->SetTarget(this);
	fFindBox->SetChangeNotifications(true);
	fFindBox->MakeFocus(true);
}


void
FindResultsWindow::MessageReceived(BMessage *msg)
{
	BMenuItem *item = NULL;
	switch (msg->what)
	{
		case M_FIND:
		{
			SpawnThread(THREAD_FIND);
			break;
		}
		case M_REPLACE:
		{
			SpawnThread(THREAD_REPLACE);
			break;
		}
		case M_REPLACE_ALL:
		{
			SpawnThread(THREAD_REPLACE_ALL);
			break;
		}
		case M_TOGGLE_REGEX:
		{
			fIsRegEx = !fIsRegEx;
			item = fMenuBar->FindItem("Regular Expression");
			item->SetMarked(fIsRegEx);
			break;
		}
		case M_TOGGLE_CASE_INSENSITIVE:
		{
			fMatchCase = !fMatchCase;
			item = fMenuBar->FindItem("Ignore Case");
			item->SetMarked(!fMatchCase);
			break;
		}
		case M_TOGGLE_MATCH_WORD:
		{
			fMatchWord = !fMatchWord;
			item = fMenuBar->FindItem("Match Whole Word");
			item->SetMarked(fMatchWord);
			break;
		}
		case M_FIND_CHANGED:
		{
			if (fFindBox->Text() && strlen(fFindBox->Text()) > 0)
				fFindButton->SetEnabled(true);
			else
				fFindButton->SetEnabled(false);
			break;
		}
		default:
		{
			DWindow::MessageReceived(msg);
			break;
		}
	}
}


void
FindResultsWindow::SpawnThread(int8 findMode)
{
	AbortThread();
	
	fThreadID = spawn_thread(FinderThread, "search_thread", B_NORMAL_PRIORITY, this);
	if (fThreadID > 0)
	{
		fThreadMode = findMode;
		resume_thread(fThreadID);
	}
	else
		fThreadID = -1;
}


void
FindResultsWindow::AbortThread(void)
{
	if (fThreadID > 0)
	{
		atomic_add(&fThreadQuitFlag, 1);
		int32 out;
		wait_for_thread(fThreadID, &out);
		
		// fThreadQuitID and fThreadID are reset by the extra thread
	}
}


int32
FindResultsWindow::FinderThread(void *data)
{
	FindResultsWindow *win = static_cast<FindResultsWindow*>(data);
	if (win)
	{
		win->Lock();
		int8 mode = win->fThreadMode;
		win->Unlock();
		
		switch (mode)
		{
			case THREAD_FIND:
			{
				win->FindResults();
				break;
			}
			case THREAD_REPLACE:
			{
				win->Replace();
				break;
			}
			case THREAD_REPLACE_ALL:
			{
				win->ReplaceAll();
				break;
			}
			default:
				break;
		}
	}
	
	win->Lock();
	win->fThreadID = -1;
	win->fThreadQuitFlag = 0;
	win->Unlock();
	
	return 0;
}


void
FindResultsWindow::FindResults(void)
{
	// This function is called from the FinderThread function, so locking is
	// required when accessing any member variables.
	Lock();
	EnableReplace(false);
	for (int32 i = fResultList->CountItems() - 1; i >= 0; i--)
	{
		// We don't want to hog the window lock, but we also don't want
		// tons of context switches, either. Yielding the lock every 5 items
		// should give us sufficient responsiveness.
		if (i % 5 == 0)
		{
			Unlock();
			Lock();
		}
		
		BListItem *item = fResultList->RemoveItem(i);
		delete item;
	}
	Unlock();


	ShellHelper shell;
	shell << "cd";
	shell.AddEscapedArg(fWorkingDir.GetFullPath());
	shell << ";" << "grep" << "-n";
	
	if (!fMatchCase)
		shell << "-i";
	
	if (fMatchWord)
		shell << "-w";
	
	if (!fIsRegEx)
		shell << "-F";
	
	shell.AddEscapedArg(fFindBox->Text());
	
	for (int32 i = 0; i < fFileList.CountItems(); i++)
	{
		BString *item = fFileList.ItemAt(i);
		shell.AddEscapedArg(item->String());
	}
	
	if (fThreadQuitFlag)
		return;
	
	BString out;
	shell.RunInPipe(out, false);
	
	if (fThreadQuitFlag)
		return;
	
	BObjectList<BString> resultList(20, true);
	TokenizeToList(out.String(), resultList);
	
	Lock();
	for (int32 i = 0; i < resultList.CountItems(); i++)
	{
		// We don't want to hog the window lock, but we also don't want
		// tons of context switches, either. Yielding the lock every 5 items
		// should give us sufficient responsiveness.
		if (i % 5 == 0)
		{
			Unlock();
			Lock();
		}
		
		//*****************************************************
		// TODO: properly add file name using a RefItem or a subclass of it
		fResultList->AddItem(new BStringItem(resultList.ItemAt(i)->String()));
	}
	if (fResultList->CountItems() > 0)
		EnableReplace(true);
	Unlock();
	
}

void
FindResultsWindow::Replace(void)
{
	// This function is called from the FinderThread function, so locking is
	// required when accessing any member variables.
	
	// This one will require some messaging work with PalEdit.
}

void
FindResultsWindow::ReplaceAll(void)
{
	// This function is called from the FinderThread function, so locking is
	// required when accessing any member variables.
	
	// Just make sure you escape single quotes and underscores before constructing
	// the sed command
	
	Lock();
	BString errorLog;
	
	for (int32 i = 0; i < fResultList->CountItems(); i++)
	{
		BString replaceTerms;
		replaceTerms << "'s_" << fFindBox->Text() << "_" << fReplaceBox->Text()
					<< "_g";
		
		
		//*****************************************************
		// TODO: get the file name from the results
		BString infileName, outfileName;
		
		ShellHelper shell;
		shell << "cd";
		shell.AddEscapedArg(fWorkingDir.GetFullPath());
		shell << ";" << "sed" << replaceTerms << "<" << infileName << ">"
				<< outfileName;
		
		int32 outvalue = shell.Run();
		if (outvalue)
		{
			// append file name to list of files with error conditions and notify
			// user of problems at the end so as not to annoy them.
			errorLog << "\t" << infileName << "\n";
		}
		
		// Allow window updates from time to time
		if (i % 5 == 0)
		{
			Unlock();
			Lock();
		}
	}
	Unlock();
	
	if (errorLog.CountChars() > 0)
	{
		BString errorString = "The following files had problems replacing the search terms:\n";
		errorString << errorLog;
		
		BAlert *alert = new BAlert("Paladin", errorString.String(), "OK");
		alert->Go();
	}
}


void
FindResultsWindow::EnableReplace(bool value)
{
	fReplaceButton->SetEnabled(value);
	fReplaceAllButton->SetEnabled(value);
	BMenuItem *item = fMenuBar->FindItem("Replace");
	if (item)
		item->SetEnabled(value);
	item = fMenuBar->FindItem("Replace All");
	if (item)
		item->SetEnabled(value);
}
