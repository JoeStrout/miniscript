using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Miniscript.Helpers {
    internal class FileHelper {

        // from: https://stackoverflow.com/questions/1321466/securely-enforcing-user-input-file-paths-within-subdirectories
        public static string SecurePathCombine(params string[] paths) {
            string combinedPath = "";

            foreach (string path in paths) {
                string newPath = Path.Combine(combinedPath, path);

                if (!newPath.StartsWith(combinedPath))
                    return null;

                combinedPath = newPath;
            }

            if (Path.GetFullPath(combinedPath) != combinedPath)
                return null;

            return combinedPath;
        }
    }
}
