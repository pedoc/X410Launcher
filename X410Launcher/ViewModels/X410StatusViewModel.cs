﻿using CommunityToolkit.Mvvm.ComponentModel;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Xml.Serialization;
using X410Launcher.Tools;

namespace X410Launcher.ViewModels
{
    public class X410StatusViewModel : ObservableObject
    {
        public const string StatusTextReady = "Ready.";

        public const string StatusTextFetching = "Fetching packages from provider: {0}...";
        public const string StatusTextFetchFailed = "Failed to fetch packages.";
        public const string StatusTextFetchCompleted = "Fetched {0} packages.";

        public const string StatusTextDownloading = "Downloading package: {0}...";
        public const string StatusTextDownloadExpired = "The selected package has expired. Please refresh your package list.";
        public const string StatusTextDownloadFailed = "Failed to download package.";
        public const string StatusTextDownloadArchNoSupport = "Your operating system architecture, {0}, is not supported.";

        public const string StatusTextExtracting = "Extracting {0} to {1}...";
        public const string StatusTextExtractingHelper = "Extracting helper library to {0}...";
        public const string StatusTextPatching = "Patching {0}...";

        public const string StatusTextInstallFailed = "Failed to install package.";
        public const string StatusTextInstallCompleted = "Successfully installed package.";

        public const string StatusTextUninstallCompleted = "Successfully uninstalled package.";

        public const string StatusTextKilled = "Sucessfully killed X410 process.";
        public const string StatusTextStarted = "Sucessfully started X410 process.";

        public const double ProgressIndeterminate = -1;
        public const double ProgressMin = 0;
        public const double ProgressMax = 100;

        public const int DownloadMaxRetries = 128;
        public const int DownloadBufferSize = 32768;

        private string? _installedVersion;
        public string? InstalledVersion
        {
            get => _installedVersion;
            private set => SetProperty(ref _installedVersion, value);
        }

        private string? _latestVersion;
        public string? LatestVersion
        {
            get => _latestVersion;
            private set => SetProperty(ref _latestVersion, value);
        }

        private ObservableCollection<PackageInfo> _packages = new();
        public ObservableCollection<PackageInfo> Packages
        {
            get => _packages;
            private set => SetProperty(ref _packages, value);
        }

        private string _statusText = StatusTextReady;
        public string StatusText
        {
            get => _statusText;
            set => SetProperty(ref _statusText, value);
        }

        private double _progress = 0;
        public double Progress
        {
            get => _progress;
            private set
            {
                SetProperty(ref _isIndeterminate, value < 0, nameof(ProgressIsIndeterminate));
                SetProperty(ref _progress, value);
            }
        }

        private bool _isIndeterminate = false;
        public bool ProgressIsIndeterminate
        {
            get => _isIndeterminate;
        }

        private string _appId = "9PM8LP83G3L3";
        public string AppId
        {
            get => _appId;
            private set => SetProperty(ref _appId, value);
        }

        private string _api = "https://store.rg-adguard.net/api/";
        public string Api
        {
            get => _api;
            private set => SetProperty(ref _api, value);
        }

        public void RefreshInstalledVersion()
        {
            var appxManifestPath = Path.Combine(Paths.GetAppInstallPath(), "AppxManifest.xml");
            if (File.Exists(appxManifestPath))
            {
                using var stream = File.OpenRead(appxManifestPath);
                var deserializer = new XmlSerializer(typeof(Models.Appx.Package));
                var package = deserializer.Deserialize(stream) as Models.Appx.Package;
                InstalledVersion = package?.Identity?.VersionString;
            }
            else
            {
                InstalledVersion = null;
            }
        }

        public async Task RefreshAsync()
        {
            RefreshInstalledVersion();

            Packages.Clear();
            Progress = ProgressIndeterminate;
            StatusText = string.Format(StatusTextFetching, _api);

            try
            {
                var msPackage = new MicrosoftStorePackage(_appId, _api);
                await msPackage.LoadAsync();
                var desiredPackageArchitectures = RuntimeInformation.OSArchitecture switch
                {
                    Architecture.X64 => new[] { Tools.PackageArchitecture.x64, Tools.PackageArchitecture.x86, Tools.PackageArchitecture.neutral },
                    Architecture.X86 => new[] { Tools.PackageArchitecture.x86, Tools.PackageArchitecture.neutral },
                    Architecture.Arm64 => new[] { Tools.PackageArchitecture.arm64, Tools.PackageArchitecture.neutral },
                    Architecture.Arm => new[] { Tools.PackageArchitecture.arm, Tools.PackageArchitecture.neutral },
                    _ => new[] { Tools.PackageArchitecture.neutral },
                };
                Packages = new ObservableCollection<PackageInfo>(
                    msPackage.Locations.Where(p =>
                        !p.Name.StartsWith("Microsoft.VCLibs") &&
                        desiredPackageArchitectures.Contains(p.Architecture)
                    ).OrderByDescending(p => p.Version));

                LatestVersion = Packages.FirstOrDefault()?.Version.ToString();
            }
            catch
            {
                Progress = ProgressMin;
                StatusText = StatusTextFetchFailed;
                throw;
            }

            Progress = ProgressMax;
            StatusText = string.Format(StatusTextFetchCompleted, Packages.Count);
        }

        public async Task InstallPackageAsync(int index)
        {
            InstalledVersion = null;

            var selectedPackage = _packages[index];

            if (selectedPackage.ExpireTime <= DateTime.Now)
            {
                StatusText = StatusTextDownloadExpired;
                RefreshInstalledVersion();
                throw new InvalidOperationException(StatusTextDownloadExpired);
            }

            StatusText = string.Format(StatusTextDownloading, selectedPackage.Name);
            Progress = ProgressIndeterminate;
            using var packageStream = new MemoryStream();

            try
            {
                await selectedPackage.DownloadAsync((buffer, currentBytesRead, bytesRead, totalBytes) =>
                {
                    if (totalBytes > 0)
                    {
                        Progress = ProgressMin + ((ProgressMax - ProgressMin) * (bytesRead / (double)totalBytes));
                    }
                    else
                    {
                        Progress = -1;
                    }
                    packageStream.Write(buffer, 0, currentBytesRead);
                });
            }
            catch
            {
                StatusText = StatusTextDownloadFailed;
                Progress = ProgressMin;
                RefreshInstalledVersion();
                throw;
            }

            packageStream.Seek(0, SeekOrigin.Begin);

            try
            {
                switch (selectedPackage.Format)
                {
                    case PackageFormat.appxbundle:
                    case PackageFormat.msixbundle:
                        {
                            using var zipArchive = new ZipArchive(packageStream, ZipArchiveMode.Read, leaveOpen: true);
                            using var appxBundleManifestStream =
                                zipArchive.Entries.First(e => e.FullName == "AppxMetadata/AppxBundleManifest.xml").Open();
                            var serializer = new XmlSerializer(typeof(Models.AppxBundle.Bundle));
                            var bundle = (serializer.Deserialize(appxBundleManifestStream) as Models.AppxBundle.Bundle)!;
                            var architecture = RuntimeInformation.OSArchitecture switch
                            {
                                Architecture.X64 => Models.AppxBundle.PackageArchitecture.X64,
                                Architecture.X86 => Models.AppxBundle.PackageArchitecture.X86,
                                Architecture.Arm64 => Models.AppxBundle.PackageArchitecture.Arm64,
                                Architecture.Arm => Models.AppxBundle.PackageArchitecture.Arm,
                                _ => Models.AppxBundle.PackageArchitecture.Neutral
                            };
                            var package = bundle.Packages
                                    .FirstOrDefault(p =>
                                        p.Architecture == architecture &&
                                        p.Type == Models.AppxBundle.PackageType.Application);
                            if (package == null)
                            {
                                var error = string.Format(StatusTextDownloadArchNoSupport, RuntimeInformation.OSArchitecture);
                                StatusText = error;
                                Progress = ProgressMin;
                                throw new InvalidOperationException(error);
                            }
                            using var newPackageZipStream = zipArchive.Entries.First(e => e.FullName == package.FileName).Open();
                            // We don't know if disposing the old stream corrupts the zip archive.
                            // Therefore we use this temporary stream instead.
                            using var newPackageMemoryStream = new MemoryStream();
                            await newPackageZipStream.CopyToAsync(newPackageMemoryStream);
                            newPackageMemoryStream.Seek(0, SeekOrigin.Begin);

                            packageStream.SetLength(newPackageMemoryStream.Length);
                            packageStream.Seek(0, SeekOrigin.Begin);
                            await newPackageMemoryStream.CopyToAsync(packageStream);

                            // Now, we have the desired appx file.
                            packageStream.Seek(0, SeekOrigin.Begin);
                        }
                        break;
                    case PackageFormat.msix:
                    case PackageFormat.appx:
                        // Do nothing, we already have the appx stream.
                        break;
                }

                using var appxArchive = new ZipArchive(packageStream);
                var appPath = Paths.GetAppInstallPath();

                Models.Appx.Package manifest;
                using (var appxManifestStream = appxArchive.Entries
                        .First(e => e.FullName == "AppxManifest.xml").Open())
                {
                    var serializer = new XmlSerializer(typeof(Models.Appx.Package));
                    manifest = (serializer.Deserialize(appxManifestStream) as Models.Appx.Package)!;
                }

                await UninstallPackageAsync();

                var directoryInfo = Directory.CreateDirectory(appPath);
                var text = directoryInfo.FullName;
                var totalLength = packageStream.Length;
                var extractedLength = 0L;

                Progress = ProgressMin;

                foreach (var entry in appxArchive.Entries)
                {
                    var fullPath = Path.GetFullPath(Path.Combine(text, entry.FullName));

                    StatusText = string.Format(StatusTextExtracting, entry.FullName, fullPath);
                    Progress = ProgressMin + ((ProgressMax - ProgressMin) * (extractedLength / (double)totalLength));

                    if (Path.GetFileName(fullPath).Length == 0)
                    {
                        Directory.CreateDirectory(fullPath);
                    }
                    else
                    {
                        Directory.CreateDirectory(Path.GetDirectoryName(fullPath)!);
                        await Task.Run(() =>
                        {
                            entry.ExtractToFile(fullPath, overwrite: true);
                        });
                        extractedLength += entry.CompressedLength;
                    }
                }

                using (var helperStream = Assembly.GetExecutingAssembly()
                    .GetManifestResourceStream(
                        $"X410Launcher.Native.X410.{RuntimeInformation.OSArchitecture}.dll"
                ))
                {
                    if (helperStream is not null)
                    {
                        StatusText = StatusTextExtractingHelper;
                        await Task.Run(() =>
                        {
                            using var memory = new MemoryStream();
                            helperStream.CopyTo(memory);
                            File.WriteAllBytes(Paths.GetHelperDllFile(), memory.ToArray());
                        });
                    }
                }

                Progress = ProgressMax;
                StatusText = StatusTextInstallCompleted;
                RefreshInstalledVersion();
            }
            catch
            {
                Progress = ProgressMin;
                StatusText = StatusTextInstallFailed;
                RefreshInstalledVersion();
                throw;
            }
        }

        public async Task UninstallPackageAsync()
        {
            await KillAsync();

            var appPath = Paths.GetAppInstallPath();

            await Task.Run(() =>
            {
                if (Directory.Exists(appPath))
                {
                    Directory.Delete(appPath, true);
                }
            });

            RefreshInstalledVersion();

            StatusText = StatusTextUninstallCompleted;
        }

        public async Task KillAsync()
        {
            await Task.Run(() =>
            {
                foreach (var proc in Process.GetProcessesByName("X410"))
                {
                    proc.Kill();
                }
            });

            StatusText = StatusTextKilled;
        }

        public void Launch()
        {
            Launcher.Launch(Paths.GetAppFile());

            StatusText = StatusTextStarted;
        }

        private void _ExtractPatched(
            Models.Appx.ProcessorArchitecture architecture,
            string fullPath,
            byte[] data
        )
        {
            StatusText = string.Format(StatusTextPatching, fullPath);

            switch (architecture)
            {
                case Models.Appx.ProcessorArchitecture.X64:
                    {
                        var dataText = string.Concat(Array.ConvertAll(data, x => x.ToString("x2")));
                        var matches = Regex.Matches(
                            dataText,
                            // push rbp
                            "4055.{0,128}" +
                            // mov rax, 0x8000000000000000
                            // xor esi, esi
                            "48b8000000000000008033f6.{0,128}?" +
                            // mov status, si
                            // mov expiry, rax
                            "(668935(.{8}))(488905(.{8}))",
                            RegexOptions.Compiled | RegexOptions.IgnoreCase
                        );

                        if (matches.Count != 1 && Debugger.IsAttached)
                        {
                            Debugger.Break();
                        }

                        foreach (var match in matches.Cast<Match>())
                        {
                            // All indices from the hex string are to be divided by two.
                            var ip0 = match.Groups[1].Index / 2;
                            var rel0 = _HexToUInt32(_StringToHex(match.Groups[2].Value).Reverse());
                            var ip1 = match.Groups[3].Index / 2;
                            var rel1 = _HexToUInt32(_StringToHex(match.Groups[4].Value).Reverse());

                            var index = match.Index / 2;

                            var head = new byte[]
                            {
                                // push rbp
                                0x40, 0x55,
                                // push rsi
                                0x56,
                                // mov rax, 0x7FFFFFFFFFFFFFFF
                                0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F,
                                // mov si, 0x1
                                0x66, 0xBE, 0x01, 0x00
                            };
                            Array.Copy(head, 0, data, index, head.Length);
                            index += head.Length;

                            rel0 += unchecked((uint)(ip0 - index));
                            var mov0 = new byte[]
                            {
                                // mov [memory location], si
                                0x66, 0x89, 0x35
                            };
                            Array.Copy(mov0, 0, data, index, mov0.Length);
                            index += mov0.Length;

                            var movAddr0 = BitConverter.GetBytes(rel0);
                            if (!BitConverter.IsLittleEndian)
                            {
                                movAddr0 = [.. movAddr0.Reverse()];
                            }
                            Array.Copy(movAddr0, 0, data, index, movAddr0.Length);
                            index += movAddr0.Length;

                            rel1 += unchecked((uint)(ip1 - index));
                            var mov1 = new byte[]
                            {
                                // mov [memory location], rax
                                0x48, 0x89, 0x05
                            };
                            Array.Copy(mov1, 0, data, index, mov1.Length);
                            index += mov1.Length;

                            var movAddr1 = BitConverter.GetBytes(rel1);
                            if (!BitConverter.IsLittleEndian)
                            {
                                movAddr1 = [.. movAddr1.Reverse()];
                            }
                            Array.Copy(movAddr1, 0, data, index, movAddr1.Length);
                            index += movAddr1.Length;

                            var tail = new byte[]
                            {
                                // pop rsi
                                0x5E,
                                // pop rbp
                                0x5D,
                                // ret
                                0xC3
                            };
                            Array.Copy(tail, 0, data, index, tail.Length);
                            index += tail.Length;
                        }
                    }
                    break;
            }

            File.WriteAllBytes(fullPath, data);
        }

        private static byte[] _StringToHex(string s)
        {
            return [
                .. Enumerable.Range(0, s.Length / 2)
                    .Select(x => Convert.ToByte(s.Substring(x * 2, 2), 16))
            ];
        }

        private static uint _HexToUInt32(IEnumerable<byte> bytes)
        {
            if (BitConverter.IsLittleEndian)
            {
                bytes = [.. bytes.Reverse()];
            }
            return BitConverter.ToUInt32([..bytes], 0);
        }
    }
}
