#include "FileUtils.h"

#include <sys/stat.h>
#include <sys/types.h>

bool FileUtils::MakeDirectory(const std::string &path, bool createParent)
{
  if (path.empty())
    return false;

  struct stat st;
  if (stat(path.c_str(), &st) == 0) {
    if (S_ISDIR(st.st_mode))
      return true;
    return false;
  }

  if (createParent) {
    size_t pos = path.find_last_of('/');
    if (pos != std::string::npos) {
      std::string parent = path.substr(0, pos);
      if (!MakeDirectory(parent, true))
        return false;
    }
  }

  return mkdir(path.c_str(), 0755) == 0;
}
