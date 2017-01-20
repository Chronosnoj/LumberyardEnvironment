/** @file Util.h
	@brief Header for Utility functions
	@author Josh Coyne - Allegorithmic (josh.coyne@allegorithmic.com)
	@date 09-14-2015
	@copyright Allegorithmic. All rights reserved.
*/
#ifndef SUBSTANCE_PROCEDURALMATERIALEDITORPLUGIN_UTIL_H
#define SUBSTANCE_PROCEDURALMATERIALEDITORPLUGIN_UTIL_H
#pragma once

namespace Util
{
	inline QString ToUnixPath(const QString& strPath)
	{
		if (strPath.indexOf('\\') != -1)
		{
			QString path = strPath;
			path.replace('\\', '/');
			return path;
		}
		return strPath;
	}

	inline QString AbsolutePathToGamePath(const QString& filename)
	{
		QString dir = ToUnixPath(QString(Path::GetEditingGameDataFolder().c_str())).toLower();
		QString stdFilename = ToUnixPath(filename).toLower();
		
		int rpos = stdFilename.lastIndexOf(dir);
		if (rpos >= 0)
		{
			stdFilename = stdFilename.remove(0, rpos + dir.length() + 1);
		}

		return stdFilename;
	}
};

#endif //SUBSTANCE_PROCEDURALMATERIALEDITORPLUGIN_UTIL_H