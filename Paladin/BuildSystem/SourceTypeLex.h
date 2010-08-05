#ifndef SOURCE_TYPE_LEX_H
#define SOURCE_TYPE_LEX_H

#include "SourceFile.h"
#include "SourceType.h"

class SourceTypeLex : public SourceType
{
public:
						SourceTypeLex(void);
			int32		CountExtensions(void) const;
			BString		GetExtension(const int32 &index);
	
			SourceFile *		CreateSourceFile(const char *path);
			SourceOptionView *	CreateOptionView(void);
			BString		GetName(void) const;

private:
			
};

class SourceFileLex : public SourceFile
{
public:
						SourceFileLex(const char *path);
			bool		UsesBuild(void) const;
			bool		CheckNeedsBuild(BuildInfo &info, bool check_deps = true);
			void		Precompile(BuildInfo &info, const char *options);
			void		Compile(BuildInfo &info, const char *options);
	
			DPath		GetObjectPath(BuildInfo &info);
			void		RemoveObjects(BuildInfo &info);
};

#endif