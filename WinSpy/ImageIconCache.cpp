#include "pch.h"
#include "ImageIconCache.h"
#include "resource.h"

int ImageIconCache::GetIcon(const CString& path, HICON* phIcon) const {
	if (path.IsEmpty() || path.Find(L'\\') < 0)
		return _defIcon;

	std::wstring wspath(path);
	{
		auto it = _icons.find(wspath);
		if (it != _icons.end()) {
			int index = it->second;
			if (phIcon)
				*phIcon = _images.GetIcon(index);
			return index;
		}
	}
	WORD index = 0;
	CString spath(path);
	auto hIcon = ::ExtractAssociatedIcon(_Module.GetModuleInstance(), spath.GetBufferSetLength(MAX_PATH), &index);

	if (hIcon) {
		int index = _images.AddIcon(hIcon);
		if (phIcon)
			*phIcon = hIcon;
		_icons.insert({ wspath, index });
		return index;
	}
	return _defIcon;
}

ImageIconCache::Map::const_iterator ImageIconCache::begin() const {
	return _icons.begin();
}

ImageIconCache::Map::const_iterator ImageIconCache::end() const {
	return _icons.end();
}

ImageIconCache::ImageIconCache() {
}

void ImageIconCache::SetImageList(HIMAGELIST hil) {
	_images.Attach(hil);
	_defIcon = _images.AddIcon(AtlLoadSysIcon(IDI_APPLICATION));
}

HIMAGELIST ImageIconCache::GetImageList() const {
	return _images;
}

ImageIconCache& ImageIconCache::Get() {
	static ImageIconCache cache;
	return cache;
}

