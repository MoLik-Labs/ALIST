(() => {
  'use strict';

  const APP = 'Kova';
  const STORAGE_KEY = 'kova.session.v1';
  const I18N = {
    en: {
      skip: 'Skip to content', navOpen: 'Open navigation', home: 'Kova home', search: 'Search files or jump to path...',
      workspaceNav: 'Workspace navigation', fileExplorer: 'File explorer', layoutMode: 'Layout mode', selectedActions: 'Selected items actions', uploadProgress: 'Upload progress',
      searchAria: 'Search files and commands', switchLanguage: 'Switch language', switchTheme: 'Switch theme',
      console: 'Console', signIn: 'Sign in', signOut: 'Sign out', notice: 'Announcement', dismiss: 'Dismiss notice',
      explorer: 'File explorer', filter: 'Filter current folder', filterPlaceholder: 'Filter folder...', listView: 'List view',
      gridView: 'Grid view', refresh: 'Refresh folder', upload: 'Upload', uploadFolder: 'Upload folder', newFolder: 'New folder',
      newFile: 'New file', fetchUrl: 'Fetch URL', dropTitle: 'Drop files or folders to upload', dropSub: 'Streams directly into the current folder',
      folderNotes: 'Folder notes', raw: 'Raw', rendered: 'Rendered', admin: 'Admin console', adminSub: 'Manage mounted storages, access control, shares, tasks, and system settings.',
      backFiles: 'Back to files', overview: 'Overview', storages: 'Storages', users: 'Users', roles: 'Roles', pathRules: 'Path rules', shares: 'Shares', tasks: 'Tasks', settings: 'Settings',
      selected: '{n} selected', selectAll: 'Select all', copyTo: 'Copy to', moveTo: 'Move to', delete: 'Delete', clearSelection: 'Clear selection',
      uploading: 'Uploading…', cancel: 'Cancel', closeUploads: 'Close uploads', searchLabel: 'Search files and workspace actions',
      searchPlaceholder: 'Search files or run an action...', close: 'Close search', all: 'All', folders: 'Folders', files: 'Files', everywhere: 'Everywhere',
      navigate: 'navigate', open: 'open', closeShort: 'close', sharedLink: 'Shared link', noItems: 'No items here yet',
      noItemsDesc: 'Upload a file, create a folder, or connect a storage from the console.', emptySearch: 'No matching files',
      loading: 'Loading…', retry: 'Try again', folder: 'Folder', file: 'File', name: 'Name', size: 'Size', modified: 'Modified',
      actions: 'Actions', itemCount: '{n} items', bytesUsed: '{size} total', sortName: 'Sort by name', sortSize: 'Sort by size', sortDate: 'Sort by modified date',
      preview: 'Preview', download: 'Download', rename: 'Rename', createShare: 'Create share link', properties: 'Properties',
      homeNav: 'All files', pinned: 'Pinned', account: 'Account and security', navigation: 'Navigation', shortcuts: 'Keyboard shortcuts',
      signInTitle: 'Sign in to Kova', signInDesc: 'Use your account credentials to continue.', username: 'Username', password: 'Password',
      otpCode: 'Two-factor code (if enabled)', otpPlaceholder: '6-digit code', submitSignIn: 'Sign in', cancelAction: 'Cancel',
      authRequired: 'Authentication required', authError: 'Sign-in required to access this workspace.', passwordGate: 'Folder password',
      passwordGateDesc: 'This location requires a password. Enter it to continue.', unlock: 'Unlock', incorrectPassword: 'Password could not unlock this location.',
      createFolderTitle: 'Create folder', folderName: 'Folder name', create: 'Create', createFileTitle: 'Create a text file', fileName: 'File name', content: 'Content',
      confirmDelete: 'Delete selected items?', confirmDeleteDesc: 'This action cannot be undone.', deleteNow: 'Delete items',
      renameTitle: 'Rename item', newName: 'New name', shareTitle: 'Create a public share', sharePassword: 'Optional share password',
      expiresHours: 'Expires in hours (0 means never)', maxDownloads: 'Maximum downloads (0 means unlimited)', directoryPassword: 'Folder password, if required',
      shareCreated: 'Share link created', copied: 'Copied to clipboard', copyLink: 'Copy link', chooseFolder: 'Choose a destination folder',
      useFolder: 'Use this folder', currentFolder: 'Current folder', parentFolder: 'Parent folder', noFolders: 'No subfolders',
      uploadQueued: 'Upload queued', uploadDone: 'Upload complete', uploadFailed: 'Upload failed', uploadCanceled: 'Upload canceled',
      uploadingFile: 'Uploading {name}', uploadCount: '{done} of {total} complete', speed: '{speed}/s',
      fileTooLarge: 'File exceeds the configured upload limit.', selectFiles: 'Select files', noWrite: 'You do not have permission to write here.',
      offlineTitle: 'Fetch files from URLs', urls: 'One URL per line', addToQueue: 'Add to queue', invalidUrls: 'Enter at least one valid URL.',
      previewTitle: 'Preview', edit: 'Edit', save: 'Save changes', saved: 'Changes saved', unsupportedPreview: 'Preview is not available for this file type.',
      archive: 'Archive browser', archiveContents: 'Archive contents', decompress: 'Extract here', extractTo: 'Extract to…', archiveEmpty: 'This archive contains no visible entries.',
      imageZoom: 'Toggle image zoom', playback: 'Media playback', showMarkdown: 'Markdown preview',
      accountTitle: 'Account and security', accountDesc: 'Manage your password, two-factor authentication, sessions, shares, and tasks.',
      changePassword: 'Change password', currentUser: 'Signed in as', passwordNew: 'New password', passwordConfirm: 'Confirm password',
      passwordChanged: 'Password updated. Sign in again with the new password.', twoFactor: 'Two-factor authentication', enable2fa: 'Set up 2FA', disable2fa: 'Disable 2FA',
      verify2fa: 'Verify and enable', secret: 'Authenticator secret', authenticatorHint: 'Add this secret to an authenticator app, then enter its current code.',
      code: 'Authentication code', sessions: 'Active sessions', evictSession: 'Revoke session', currentSession: 'Current session',
      userShares: 'My shares', userTasks: 'My tasks', noSessions: 'No active sessions.', noShares: 'No shares yet.', noTasks: 'No tasks yet.',
      current: 'Current', revoke: 'Revoke', logOut: 'Sign out', sessionExpired: 'Session expired. Please sign in again.',
      fileDetails: 'File details', path: 'Path', provider: 'Storage', type: 'Type', creator: 'Created by', views: 'Views', downloads: 'Downloads',
      expires: 'Expires', protected: 'Password protected', publicShare: 'Public share', expired: 'Expired',
      telemetry: 'Live system telemetry', pause: 'Pause updates', resume: 'Resume updates', refreshNow: 'Refresh now',
      uptime: 'Uptime', memory: 'Memory', peakMemory: 'Peak memory', threads: 'Threads', cpuCores: 'CPU cores',
      activeConnections: 'Active connections', totalRequests: 'Total requests', sent: 'Bytes sent', received: 'Bytes received',
      mounted: 'Mounted storages', database: 'Database size', cache: 'VFS cache', cacheHits: 'Cache hits', cacheMisses: 'Cache misses',
      indexed: 'Indexed files', activeTasks: 'Active tasks', completedTasks: 'Completed tasks', telemetryUnavailable: 'Telemetry is not available yet.',
      addStorage: 'Add storage', reloadStorages: 'Reload all', edit: 'Edit', remove: 'Delete', mountPath: 'Mount path', driver: 'Driver',
      order: 'Order', cacheExpiration: 'Cache minutes', remark: 'Remark', config: 'Driver configuration', signUrls: 'Sign downloads',
      webProxy: 'Proxy downloads', orderBy: 'Sort field', direction: 'Sort direction', enabled: 'Enabled', disabled: 'Disabled',
      status: 'Status', save: 'Save', createStorage: 'Create storage', updateStorage: 'Update storage', noStorages: 'No storages configured.',
      storageWarning: 'Storage saved, but could not be mounted: {message}', storageSaved: 'Storage saved.', storageDeleted: 'Storage deleted.',
      addUser: 'Add user', basePath: 'Base path', rolesAssigned: 'Assigned roles', guestAccess: 'Guest access', guestEnabled: 'Guest access enabled', guestDisabled: 'Guest access disabled',
      twoFactorEnabled: '2FA enabled', reset2fa: 'Reset 2FA', deleteUser: 'Delete user', createUser: 'Create user', updateUser: 'Update user',
      noUsers: 'No users found.', roleId: 'Role ID', guest: 'Guest', administrator: 'Administrator', standardUser: 'User',
      addRole: 'Add role', roleName: 'Role name', description: 'Description', permissionScopes: 'Permission scope', scopePath: 'Scope path',
      noRoles: 'No roles found.', deleteRole: 'Delete role', seeHidden: 'See hidden files', accessWithoutPassword: 'Access without password',
      offlineDownloads: 'Add offline downloads', write: 'Write', renamePermission: 'Rename', move: 'Move', copy: 'Copy', removePermission: 'Remove',
      webdavRead: 'WebDAV read', webdavManage: 'WebDAV manage', ftpAccess: 'FTP access', ftpManage: 'FTP manage', readArchives: 'Read archives',
      decompressPermission: 'Decompress archives', pathLimit: 'Path limit', mcpAccess: 'MCP access', mcpManage: 'MCP manage',
      addRule: 'Add path rule', editRule: 'Edit path rule', noRules: 'No path rules found.', hidePatterns: 'Hidden name patterns',
      passwordSubtree: 'Password applies to subfolders', writeEnabled: 'Allow writes', writeSubtree: 'Writes apply to subfolders',
      hideSubtree: 'Hide patterns in subfolders', readmeText: 'Folder notes (Markdown)', readmeSubtree: 'Notes apply to subfolders',
      noSharesAdmin: 'No shares found.', deleteShare: 'Delete share', owner: 'Owner',
      active: 'Active', completed: 'Completed', progress: 'Progress', taskType: 'Task type', clearCompleted: 'Clear completed', noAdminTasks: 'No tasks found.',
      siteTitle: 'Site title', announcementSetting: 'Announcement', pageSize: 'Default page size', pagination: 'Pagination mode',
      allowIndexed: 'Allow indexed file search', allowMounted: 'Show mounted storage paths', robots: 'Robots.txt', searchEnabled: 'Enable search',
      searchIndex: 'Search index', signAll: 'Sign all downloads', linkExpiration: 'Download link expiration (hours)', maxDevices: 'Maximum devices per user',
      sessionTtl: 'Session lifetime (seconds)', forwardParams: 'Forward direct-link parameters', saveSettings: 'Save settings', settingsSaved: 'Settings saved.',
      resetToken: 'Reset system token', buildIndex: 'Build FTS5 index', clearIndex: 'Clear search index', indexProgress: 'Index progress',
      tokenReset: 'System token reset. Copy it now; it will not be shown again.', copyToken: 'Copy token', indexedCount: '{n} indexed files',
      settingsDesc: 'Configure public workspace behavior, session limits, download links, and full-text indexing.',
      ok: 'OK', error: 'Error', success: 'Success', warning: 'Warning', operationFailed: 'The operation could not be completed.', backendUnavailable: 'Kova server is unavailable. This static page cannot connect to files or sign in until a compatible backend is running.',
      viewShortcuts: 'Show keyboard shortcuts', shortcutAction: 'Action', shortcutKey: 'Shortcut',
      shortcutSearch: 'Search and commands', shortcutFilter: 'Filter this folder', shortcutMove: 'Move between files', shortcutOpen: 'Open or preview',
      shortcutParent: 'Go to parent folder', shortcutSelect: 'Select focused item', shortcutRename: 'Rename selected item', shortcutDelete: 'Delete selected items', shortcutHelp: 'Show this help',
      notFound: 'The requested item was not found.', item: 'Item', unknown: 'Unknown', never: 'Never', bytes: 'bytes',
      choose: 'Choose', clear: 'Clear', action: 'Action', allFiles: 'All files', filesSelected: 'Selected files', folderSelected: 'Selected folder',
      sortAscending: 'Ascending', sortDescending: 'Descending', noResults: 'No results. Try a different query.',
      viewAdminTab: 'Open console', fileUpload: 'Upload file', folderUpload: 'Upload folder', copiedPath: 'Path copied',
      driverLocal: 'Local filesystem', driverS3: 'S3-compatible storage', driverWebDAV: 'WebDAV storage', driverGoogleDrive: 'Google Drive',
      driverOneDrive: 'OneDrive', driverDropbox: 'Dropbox', driverSFTP: 'SFTP', driverFTP: 'FTP', driverURLTree: 'URL tree', driverAlias: 'Alias mount', driverCrypt: 'Encrypted vault',
      confirm: 'Confirm', remove: 'Remove', noPermission: 'You do not have permission to perform this action.',
    },
    vi: {
      skip: 'Bỏ qua đến nội dung', navOpen: 'Mở điều hướng', home: 'Trang chủ Kova', search: 'Tìm tệp hoặc đến đường dẫn...',
      workspaceNav: 'Điều hướng không gian làm việc', fileExplorer: 'Trình quản lý tệp', layoutMode: 'Kiểu bố cục', selectedActions: 'Thao tác với mục đã chọn', uploadProgress: 'Tiến độ tải lên',
      searchAria: 'Tìm tệp và lệnh', switchLanguage: 'Đổi ngôn ngữ', switchTheme: 'Đổi giao diện', console: 'Bảng quản trị', signIn: 'Đăng nhập', signOut: 'Đăng xuất',
      notice: 'Thông báo', dismiss: 'Đóng thông báo', explorer: 'Trình quản lý tệp', filter: 'Lọc thư mục hiện tại', filterPlaceholder: 'Lọc thư mục...',
      listView: 'Dạng danh sách', gridView: 'Dạng lưới', refresh: 'Tải lại thư mục', upload: 'Tải lên', uploadFolder: 'Tải thư mục lên', newFolder: 'Thư mục mới',
      newFile: 'Tệp mới', fetchUrl: 'Tải từ URL', dropTitle: 'Thả tệp hoặc thư mục để tải lên', dropSub: 'Truyền trực tiếp vào thư mục hiện tại',
      folderNotes: 'Ghi chú thư mục', raw: 'Văn bản gốc', rendered: 'Đã định dạng', admin: 'Bảng quản trị', adminSub: 'Quản lý kho lưu trữ, quyền truy cập, liên kết chia sẻ, tác vụ và cài đặt hệ thống.',
      backFiles: 'Quay lại tệp', overview: 'Tổng quan', storages: 'Kho lưu trữ', users: 'Người dùng', roles: 'Vai trò', pathRules: 'Quy tắc đường dẫn', shares: 'Chia sẻ', tasks: 'Tác vụ', settings: 'Cài đặt',
      selected: 'Đã chọn {n}', selectAll: 'Chọn tất cả', copyTo: 'Sao chép đến', moveTo: 'Di chuyển đến', delete: 'Xóa', clearSelection: 'Bỏ chọn',
      uploading: 'Đang tải lên…', cancel: 'Hủy', closeUploads: 'Đóng danh sách tải lên', searchLabel: 'Tìm tệp và thao tác',
      searchPlaceholder: 'Tìm tệp hoặc chạy lệnh...', close: 'Đóng tìm kiếm', all: 'Tất cả', folders: 'Thư mục', files: 'Tệp', everywhere: 'Mọi nơi',
      navigate: 'di chuyển', open: 'mở', closeShort: 'đóng', sharedLink: 'Liên kết chia sẻ', noItems: 'Thư mục này chưa có nội dung',
      noItemsDesc: 'Tải tệp lên, tạo thư mục hoặc kết nối kho lưu trữ trong bảng quản trị.', emptySearch: 'Không tìm thấy tệp phù hợp',
      loading: 'Đang tải…', retry: 'Thử lại', folder: 'Thư mục', file: 'Tệp', name: 'Tên', size: 'Kích thước', modified: 'Đã sửa đổi',
      actions: 'Thao tác', itemCount: '{n} mục', bytesUsed: 'Tổng cộng {size}', sortName: 'Sắp xếp theo tên', sortSize: 'Sắp xếp theo kích thước', sortDate: 'Sắp xếp theo ngày sửa đổi',
      preview: 'Xem trước', download: 'Tải xuống', rename: 'Đổi tên', createShare: 'Tạo liên kết chia sẻ', properties: 'Thuộc tính', homeNav: 'Tất cả tệp', pinned: 'Ghim',
      account: 'Tài khoản và bảo mật', navigation: 'Điều hướng', shortcuts: 'Phím tắt', signInTitle: 'Đăng nhập Kova', signInDesc: 'Nhập thông tin tài khoản để tiếp tục.',
      username: 'Tên người dùng', password: 'Mật khẩu', otpCode: 'Mã xác thực hai bước (nếu bật)', otpPlaceholder: 'Mã gồm 6 chữ số', submitSignIn: 'Đăng nhập', cancelAction: 'Hủy',
      authRequired: 'Cần xác thực', authError: 'Đăng nhập để truy cập không gian làm việc này.', passwordGate: 'Mật khẩu thư mục',
      passwordGateDesc: 'Vị trí này yêu cầu mật khẩu. Nhập mật khẩu để tiếp tục.', unlock: 'Mở khóa', incorrectPassword: 'Không thể mở khóa vị trí này bằng mật khẩu đã nhập.',
      createFolderTitle: 'Tạo thư mục', folderName: 'Tên thư mục', create: 'Tạo', createFileTitle: 'Tạo tệp văn bản', fileName: 'Tên tệp', content: 'Nội dung',
      confirmDelete: 'Xóa các mục đã chọn?', confirmDeleteDesc: 'Thao tác này không thể hoàn tác.', deleteNow: 'Xóa mục', renameTitle: 'Đổi tên mục', newName: 'Tên mới',
      shareTitle: 'Tạo liên kết chia sẻ công khai', sharePassword: 'Mật khẩu chia sẻ (không bắt buộc)', expiresHours: 'Hết hạn sau (giờ, 0 là không giới hạn)',
      maxDownloads: 'Số lượt tải tối đa (0 là không giới hạn)', directoryPassword: 'Mật khẩu thư mục nếu có', shareCreated: 'Đã tạo liên kết chia sẻ', copied: 'Đã sao chép vào bộ nhớ tạm',
      copyLink: 'Sao chép liên kết', chooseFolder: 'Chọn thư mục đích', useFolder: 'Chọn thư mục này', currentFolder: 'Thư mục hiện tại', parentFolder: 'Thư mục cha', noFolders: 'Không có thư mục con',
      uploadQueued: 'Đã xếp hàng tải lên', uploadDone: 'Tải lên hoàn tất', uploadFailed: 'Tải lên thất bại', uploadCanceled: 'Đã hủy tải lên',
      uploadingFile: 'Đang tải {name}', uploadCount: 'Hoàn tất {done}/{total}', speed: '{speed}/giây', fileTooLarge: 'Tệp vượt quá giới hạn tải lên đã cấu hình.',
      selectFiles: 'Chọn tệp', noWrite: 'Bạn không có quyền ghi tại đây.', offlineTitle: 'Tải tệp từ URL', urls: 'Mỗi dòng một URL', addToQueue: 'Thêm vào hàng đợi', invalidUrls: 'Nhập ít nhất một URL hợp lệ.',
      previewTitle: 'Xem trước', edit: 'Chỉnh sửa', save: 'Lưu thay đổi', saved: 'Đã lưu thay đổi', unsupportedPreview: 'Không hỗ trợ xem trước loại tệp này.',
      archive: 'Duyệt tệp nén', archiveContents: 'Nội dung tệp nén', decompress: 'Giải nén tại đây', extractTo: 'Giải nén đến…', archiveEmpty: 'Tệp nén không có mục hiển thị.',
      imageZoom: 'Phóng to ảnh', playback: 'Phát nội dung', showMarkdown: 'Xem Markdown', accountTitle: 'Tài khoản và bảo mật',
      accountDesc: 'Quản lý mật khẩu, xác thực hai bước, phiên đăng nhập, chia sẻ và tác vụ.', changePassword: 'Đổi mật khẩu', currentUser: 'Đang đăng nhập với',
      passwordNew: 'Mật khẩu mới', passwordConfirm: 'Xác nhận mật khẩu', passwordChanged: 'Đã cập nhật mật khẩu. Hãy đăng nhập lại bằng mật khẩu mới.',
      twoFactor: 'Xác thực hai bước', enable2fa: 'Thiết lập 2FA', disable2fa: 'Tắt 2FA', verify2fa: 'Xác minh và bật', secret: 'Khóa xác thực',
      authenticatorHint: 'Thêm khóa này vào ứng dụng xác thực, sau đó nhập mã hiện tại.', code: 'Mã xác thực', sessions: 'Phiên đăng nhập', evictSession: 'Thu hồi phiên',
      currentSession: 'Phiên hiện tại', userShares: 'Liên kết của tôi', userTasks: 'Tác vụ của tôi', noSessions: 'Không có phiên đang hoạt động.', noShares: 'Chưa có liên kết chia sẻ.',
      noTasks: 'Chưa có tác vụ.', current: 'Hiện tại', revoke: 'Thu hồi', logOut: 'Đăng xuất', sessionExpired: 'Phiên đã hết hạn. Vui lòng đăng nhập lại.',
      fileDetails: 'Thông tin tệp', path: 'Đường dẫn', provider: 'Kho lưu trữ', type: 'Loại', creator: 'Người tạo', views: 'Lượt xem', downloads: 'Lượt tải', expires: 'Hết hạn', protected: 'Có mật khẩu',
      publicShare: 'Chia sẻ công khai', expired: 'Đã hết hạn', telemetry: 'Theo dõi hệ thống trực tiếp', pause: 'Tạm dừng cập nhật', resume: 'Tiếp tục cập nhật', refreshNow: 'Làm mới',
      uptime: 'Thời gian hoạt động', memory: 'Bộ nhớ', peakMemory: 'Bộ nhớ đỉnh', threads: 'Luồng', cpuCores: 'Lõi CPU', activeConnections: 'Kết nối hiện tại',
      totalRequests: 'Tổng yêu cầu', sent: 'Đã gửi', received: 'Đã nhận', mounted: 'Kho đã gắn', database: 'Dung lượng cơ sở dữ liệu', cache: 'Bộ nhớ đệm VFS', cacheHits: 'Lượt trúng bộ đệm',
      cacheMisses: 'Lượt trượt bộ đệm', indexed: 'Tệp đã lập chỉ mục', activeTasks: 'Tác vụ đang chạy', completedTasks: 'Tác vụ hoàn tất', telemetryUnavailable: 'Chưa có dữ liệu theo dõi.',
      addStorage: 'Thêm kho lưu trữ', reloadStorages: 'Tải lại tất cả', mountPath: 'Đường dẫn gắn', driver: 'Trình điều khiển', order: 'Thứ tự', cacheExpiration: 'Thời hạn bộ đệm (phút)',
      remark: 'Ghi chú', config: 'Cấu hình trình điều khiển', signUrls: 'Ký liên kết tải xuống', webProxy: 'Proxy tải xuống', orderBy: 'Trường sắp xếp', direction: 'Chiều sắp xếp',
      enabled: 'Đã bật', disabled: 'Đã tắt', status: 'Trạng thái', createStorage: 'Tạo kho lưu trữ', updateStorage: 'Cập nhật kho lưu trữ', noStorages: 'Chưa cấu hình kho lưu trữ nào.',
      storageWarning: 'Đã lưu kho nhưng không thể gắn: {message}', storageSaved: 'Đã lưu kho lưu trữ.', storageDeleted: 'Đã xóa kho lưu trữ.',
      addUser: 'Thêm người dùng', basePath: 'Đường dẫn gốc', rolesAssigned: 'Vai trò được gán', guestAccess: 'Truy cập khách', guestEnabled: 'Đã bật truy cập khách', guestDisabled: 'Đã tắt truy cập khách',
      twoFactorEnabled: 'Đã bật 2FA', reset2fa: 'Đặt lại 2FA', deleteUser: 'Xóa người dùng', createUser: 'Tạo người dùng', updateUser: 'Cập nhật người dùng', noUsers: 'Không tìm thấy người dùng.',
      roleId: 'Mã vai trò', guest: 'Khách', administrator: 'Quản trị viên', standardUser: 'Người dùng', addRole: 'Thêm vai trò', roleName: 'Tên vai trò', description: 'Mô tả',
      permissionScopes: 'Phạm vi quyền', scopePath: 'Đường dẫn phạm vi', noRoles: 'Không tìm thấy vai trò.', deleteRole: 'Xóa vai trò', seeHidden: 'Xem tệp ẩn', accessWithoutPassword: 'Truy cập không cần mật khẩu',
      offlineDownloads: 'Thêm tải ngoại tuyến', write: 'Ghi', renamePermission: 'Đổi tên', move: 'Di chuyển', copy: 'Sao chép', removePermission: 'Xóa', webdavRead: 'Đọc WebDAV',
      webdavManage: 'Quản lý WebDAV', ftpAccess: 'Truy cập FTP', ftpManage: 'Quản lý FTP', readArchives: 'Đọc tệp nén', decompressPermission: 'Giải nén', pathLimit: 'Giới hạn đường dẫn',
      mcpAccess: 'Truy cập MCP', mcpManage: 'Quản lý MCP', addRule: 'Thêm quy tắc đường dẫn', editRule: 'Sửa quy tắc đường dẫn', noRules: 'Không tìm thấy quy tắc đường dẫn.',
      hidePatterns: 'Mẫu tên bị ẩn', passwordSubtree: 'Mật khẩu áp dụng cho thư mục con', writeEnabled: 'Cho phép ghi', writeSubtree: 'Ghi áp dụng cho thư mục con',
      hideSubtree: 'Ẩn mẫu tên trong thư mục con', readmeText: 'Ghi chú thư mục (Markdown)', readmeSubtree: 'Ghi chú áp dụng cho thư mục con', noSharesAdmin: 'Không tìm thấy liên kết chia sẻ.', deleteShare: 'Xóa liên kết', owner: 'Chủ sở hữu',
      active: 'Đang chạy', completed: 'Hoàn tất', progress: 'Tiến độ', taskType: 'Loại tác vụ', clearCompleted: 'Xóa tác vụ hoàn tất', noAdminTasks: 'Không tìm thấy tác vụ.',
      siteTitle: 'Tiêu đề trang', announcementSetting: 'Thông báo', pageSize: 'Số mục mặc định', pagination: 'Kiểu phân trang', allowIndexed: 'Cho phép tìm kiếm chỉ mục', allowMounted: 'Hiển thị đường dẫn kho đã gắn',
      robots: 'Robots.txt', searchEnabled: 'Bật tìm kiếm', searchIndex: 'Chỉ mục tìm kiếm', signAll: 'Ký mọi lượt tải', linkExpiration: 'Thời hạn liên kết tải (giờ)',
      maxDevices: 'Số thiết bị tối đa mỗi người dùng', sessionTtl: 'Thời hạn phiên (giây)', forwardParams: 'Chuyển tiếp tham số liên kết trực tiếp', saveSettings: 'Lưu cài đặt', settingsSaved: 'Đã lưu cài đặt.',
      resetToken: 'Đặt lại mã hệ thống', buildIndex: 'Tạo chỉ mục FTS5', clearIndex: 'Xóa chỉ mục tìm kiếm', indexProgress: 'Tiến độ chỉ mục', tokenReset: 'Đã đặt lại mã hệ thống. Hãy sao chép ngay, mã sẽ không hiển thị lại.',
      copyToken: 'Sao chép mã', indexedCount: 'Đã lập chỉ mục {n} tệp', settingsDesc: 'Cấu hình hành vi công khai, giới hạn phiên, liên kết tải và lập chỉ mục toàn văn.',
      ok: 'Đồng ý', error: 'Lỗi', success: 'Thành công', warning: 'Cảnh báo', operationFailed: 'Không thể hoàn tất thao tác.', backendUnavailable: 'Không kết nối được máy chủ Kova. Trang tĩnh chưa thể đăng nhập hoặc truy cập tệp cho đến khi backend tương thích hoạt động.', viewShortcuts: 'Xem phím tắt', shortcutAction: 'Thao tác', shortcutKey: 'Phím tắt',
      shortcutSearch: 'Tìm kiếm và lệnh', shortcutFilter: 'Lọc thư mục', shortcutMove: 'Di chuyển giữa các tệp', shortcutOpen: 'Mở hoặc xem trước', shortcutParent: 'Đến thư mục cha',
      shortcutSelect: 'Chọn mục đang focus', shortcutRename: 'Đổi tên mục đã chọn', shortcutDelete: 'Xóa mục đã chọn', shortcutHelp: 'Hiển thị trợ giúp', notFound: 'Không tìm thấy mục được yêu cầu.', item: 'Mục', unknown: 'Không rõ', never: 'Không bao giờ', bytes: 'byte',
      choose: 'Chọn', clear: 'Xóa', action: 'Thao tác', allFiles: 'Tất cả tệp', filesSelected: 'Tệp đã chọn', folderSelected: 'Thư mục đã chọn', sortAscending: 'Tăng dần', sortDescending: 'Giảm dần', noResults: 'Không có kết quả. Hãy thử truy vấn khác.',
      viewAdminTab: 'Mở bảng quản trị', fileUpload: 'Tải tệp lên', folderUpload: 'Tải thư mục lên', copiedPath: 'Đã sao chép đường dẫn', driverLocal: 'Hệ thống tệp cục bộ', driverS3: 'Kho tương thích S3', driverWebDAV: 'Kho WebDAV',
      driverGoogleDrive: 'Google Drive', driverOneDrive: 'OneDrive', driverDropbox: 'Dropbox', driverSFTP: 'SFTP', driverFTP: 'FTP', driverURLTree: 'Cây URL', driverAlias: 'Gắn bí danh', driverCrypt: 'Kho mã hóa', confirm: 'Xác nhận', remove: 'Gỡ bỏ', noPermission: 'Bạn không có quyền thực hiện thao tác này.',
    }
  };

  const TEXT_IDS = {
    skipLink: 'skip', btnMobileNav: 'navOpen', cmdTriggerLabel: 'search', btnLang: null,
    btnTopAdminLabel: 'console', btnAuth: 'signIn', announcementText: null,
    filterInput: null, btnViewList: 'listView', btnViewGrid: 'gridView', btnRefresh: 'refresh',
    lblUpload: 'upload', lblUploadFolder: 'uploadFolder', lblMkdir: 'newFolder', lblNewFile: 'newFile', lblOffline: 'fetchUrl',
    dropTitle: 'dropTitle', dropSub: 'dropSub', readmeHeading: 'folderNotes', btnReadmeMode: 'raw',
    adminHeading: 'admin', adminSub: 'adminSub', lblBackFiles: 'backFiles', tabOverview: 'overview', tabStorages: 'storages',
    tabUsers: 'users', tabRoles: 'roles', tabMetas: 'pathRules', tabShares: 'shares', tabTasks: 'tasks', tabSettings: 'settings',
    btnBatchAll: 'selectAll', btnBatchCopy: 'copyTo', btnBatchMove: 'moveTo', btnBatchDelete: 'delete',
    btnUploadCancel: 'cancel', paletteInputLabel: 'searchLabel', scopeAll: 'all', scopeDirs: 'folders', scopeFiles: 'files', scopeGlobalLabel: 'everywhere'
  };

  const $ = id => document.getElementById(id);
  const esc = value => String(value ?? '').replace(/[&<>"']/g, ch => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[ch]));
  const readStore = (key, fallback = '') => { try { return localStorage.getItem(key) ?? fallback; } catch { return fallback; } };
  const writeStore = (key, value) => { try { localStorage.setItem(key, value); } catch {} };
  const removeStore = key => { try { localStorage.removeItem(key); } catch {} };
  const safeArray = value => Array.isArray(value) ? value : [];
  const pathJoin = (base, part) => {
    const raw = `${base || '/'}/${part || ''}`.replace(/\\/g, '/');
    const out = [];
    for (const seg of raw.split('/')) {
      if (!seg || seg === '.') continue;
      if (seg === '..') { out.pop(); continue; }
      out.push(seg);
    }
    return `/${out.join('/')}`;
  };
  const parentPath = path => {
    const clean = pathJoin('/', path);
    if (clean === '/') return '/';
    const pos = clean.lastIndexOf('/');
    return pos <= 0 ? '/' : clean.slice(0, pos);
  };
  const encodePath = path => pathJoin('/', path).split('/').map((segment, index) => index ? encodeURIComponent(segment) : '').join('/');
  const normalizedSearch = text => String(text ?? '').normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLocaleLowerCase();
  const formatBytes = bytes => {
    const n = Number(bytes);
    if (!Number.isFinite(n) || n <= 0) return '—';
    const units = ['B', 'KB', 'MB', 'GB', 'TB', 'PB'];
    const index = Math.min(Math.floor(Math.log(n) / Math.log(1024)), units.length - 1);
    return `${(n / (1024 ** index)).toLocaleString(undefined, { maximumFractionDigits: index ? 1 : 0 })} ${units[index]}`;
  };
  const formatDate = value => {
    if (!value) return '—';
    let date = typeof value === 'number' ? new Date(value < 100000000000 ? value * 1000 : value) : new Date(value);
    if (Number.isNaN(date.getTime())) return '—';
    return new Intl.DateTimeFormat(state.lang === 'vi' ? 'vi-VN' : 'en', { dateStyle: 'medium', timeStyle: 'short' }).format(date);
  };
  const formatDuration = seconds => {
    let left = Math.max(0, Number(seconds) || 0);
    const d = Math.floor(left / 86400); left %= 86400;
    const h = Math.floor(left / 3600); left %= 3600;
    const m = Math.floor(left / 60); const s = Math.floor(left % 60);
    return [d && `${d}d`, (d || h) && `${h}h`, (d || h || m) && `${m}m`, `${s}s`].filter(Boolean).join(' ');
  };
  const t = (key, vars = {}) => {
    const template = I18N[state.lang]?.[key] ?? I18N.en[key] ?? key;
    return template.replace(/\{([a-zA-Z0-9_]+)\}/g, (_, name) => String(vars[name] ?? ''));
  };
  const langName = key => t(key);
  const getToken = () => readStore(STORAGE_KEY, '');
  const clientId = (() => {
    let value = readStore('kova.client.v1', '');
    if (!value) {
      value = (globalThis.crypto?.randomUUID?.() || `${Date.now().toString(36)}-${Math.random().toString(36).slice(2)}`);
      writeStore('kova.client.v1', value);
    }
    return value;
  })();

  const state = {
    lang: readStore('kova.language', 'en') === 'vi' ? 'vi' : 'en',
    theme: ['light', 'dark', 'system'].includes(readStore('kova.theme', 'system')) ? readStore('kova.theme', 'system') : 'system',
    token: getToken(), user: null, settings: {}, currentView: 'explorer', path: '/', items: [], filtered: [],
    listMeta: null, rootItems: [], canWrite: false, filter: '', view: readStore('kova.view', 'list') === 'grid' ? 'grid' : 'list',
    sort: { key: 'name', dir: 1 }, renderLimit: 160, focusIndex: -1, selected: new Set(),
    listController: null, searchController: null, palette: { scope: 0, global: true, index: 0, timer: 0, results: [] },
    pathPasswords: {}, readmeRaw: false, modalReturnFocus: null, previewUrls: [], previewMode: 'view',
    contextPath: '', uploadQueue: [], uploadXhr: null, uploadCancel: false, uploadStarted: 0, uploadBytes: 0,
    picker: null, adminTab: 'overview', adminTimer: 0, telemetryPaused: false, adminData: {},
    share: null, shareSubpath: '/', shareItems: [], shareController: null, shareToken: '', shareUnlocked: false,
    archive: null, storageForm: null, roleScopes: [], backendUnavailable: false, announcementDismissed: readStore('kova.notice.dismissed', '') === '1'
  };

  class ApiError extends Error {
    constructor(message, code = 0, status = 0) { super(message); this.name = 'ApiError'; this.code = code; this.status = status; }
  }
  function jsonHeaders(extra = {}) {
    const h = { Accept: 'application/json', 'Client-Id': clientId, ...extra };
    if (state.token) h.Authorization = `Bearer ${state.token}`;
    return h;
  }
  async function api(url, options = {}) {
    const headers = jsonHeaders(options.headers || {});
    const init = { method: options.method || (options.body === undefined ? 'GET' : 'POST'), headers, signal: options.signal, credentials: 'same-origin' };
    if (options.body !== undefined) {
      if (typeof options.body === 'string' || options.body instanceof Blob || options.body instanceof FormData) init.body = options.body;
      else { headers['Content-Type'] = 'application/json'; init.body = JSON.stringify(options.body); }
    }
    let response;
    try { response = await fetch(url, init); }
    catch (error) {
      if (error.name === 'AbortError') throw error;
      throw new ApiError(error.message || t('operationFailed'));
    }
    let payload;
    try { payload = await response.json(); }
    catch { throw new ApiError(response.statusText || t('operationFailed'), response.status, response.status); }
    if (!response.ok || (payload && Number(payload.code) !== 200)) {
      const code = Number(payload?.code || response.status || 0);
      const err = new ApiError(payload?.message || response.statusText || t('operationFailed'), code, response.status);
      if (code === 401 && !url.includes('/api/auth/login') && !url.includes('/api/public/')) {
        if (state.token) { state.token = ''; state.user = null; removeStore(STORAGE_KEY); updateAuthButton(); }
      }
      throw err;
    }
    return payload?.data;
  }
  async function fetchResource(url, { share = false, password = '' } = {}) {
    const headers = {};
    if (!share && state.token) headers.Authorization = `Bearer ${state.token}`;
    headers['Client-Id'] = clientId;
    if (password) headers.Password = password;
    let response;
    try { response = await fetch(url, { headers, credentials: 'same-origin' }); }
    catch (error) { throw new ApiError(error.message || t('operationFailed')); }
    if (!response.ok) {
      const type = response.headers.get('content-type') || '';
      let message = response.statusText || t('operationFailed');
      if (type.includes('json')) {
        try { const payload = await response.json(); message = payload.message || message; } catch {}
      }
      throw new ApiError(message, response.status, response.status);
    }
    return response;
  }
  function toast(message, kind = 'ok', timeout = 3600) {
    const stack = $('toastStack');
    if (!stack) return;
    const node = document.createElement('div');
    node.className = `toast-item ${kind === 'err' ? 'is-err' : kind === 'ok' ? 'is-ok' : ''}`;
    node.setAttribute('role', kind === 'err' ? 'alert' : 'status');
    node.textContent = String(message || t('operationFailed'));
    stack.appendChild(node);
    while (stack.children.length > 4) stack.firstElementChild.remove();
    setTimeout(() => node.remove(), timeout);
  }
  function setTheme(theme) {
    state.theme = theme;
    writeStore('kova.theme', theme);
    const root = document.documentElement;
    if (theme === 'system') root.removeAttribute('data-theme'); else root.setAttribute('data-theme', theme);
    const icon = $('themeIcon');
    if (icon) icon.innerHTML = theme === 'dark'
      ? '<path d="M16.2 12.4A6.7 6.7 0 0 1 7.6 3.8 6.8 6.8 0 1 0 16.2 12.4Z"/>'
      : theme === 'light'
        ? '<circle cx="10" cy="10" r="4"/><path d="M10 2v2M10 16v2M2 10h2M16 10h2M4.3 4.3l1.4 1.4M14.3 14.3l1.4 1.4M15.7 4.3l-1.4 1.4M5.7 14.3l-1.4 1.4"/>'
        : '<rect x="3" y="3" width="14" height="14" rx="3"/><path d="M10 3v14"/>';
    $('btnTheme')?.setAttribute('aria-label', `${t('switchTheme')}: ${theme}`);
    $('metaThemeColor')?.setAttribute('content', theme === 'dark' ? '#0a0d12' : '#f3f5f7');
  }
  function applyLanguage() {
    document.documentElement.lang = state.lang;
    for (const [id, key] of Object.entries(TEXT_IDS)) {
      if (!key || !$(id)) continue;
      if (id.startsWith('btnView') || id === 'btnRefresh' || id === 'btnMobileNav') $(id).setAttribute('aria-label', t(key));
      else $(id).textContent = t(key);
    }
    $('btnLang').textContent = state.lang.toUpperCase();
    $('btnLang').setAttribute('aria-label', t('switchLanguage'));
    $('btnTheme')?.setAttribute('aria-label', `${t('switchTheme')}: ${state.theme}`);
    $('btnAuth').textContent = state.user ? state.user.username : t('signIn');
    $('cmdTrigger').setAttribute('aria-label', t('searchAria'));
    document.querySelector('.brand')?.setAttribute('aria-label', t('home'));
    $('appSidebar').setAttribute('aria-label', t('workspaceNav'));
    $('explorerView').setAttribute('aria-label', t('fileExplorer'));
    document.querySelector('.view-switch')?.setAttribute('aria-label', t('layoutMode'));
    $('batchDock').setAttribute('aria-label', t('selectedActions'));
    $('uploadDock').setAttribute('aria-label', t('uploadProgress'));
    $('announcementBar').setAttribute('aria-label', t('notice'));
    if (state.backendUnavailable) $('announcementText').textContent = t('backendUnavailable');
    $('filterInput').setAttribute('placeholder', t('filterPlaceholder'));
    $('filterInput').setAttribute('aria-label', t('filter'));
    $('paletteInput').setAttribute('placeholder', t('searchPlaceholder'));
    $('paletteInput').setAttribute('aria-label', t('searchLabel'));
    $('btnMobileNav').setAttribute('aria-label', t('navOpen'));
    $('btnTheme').setAttribute('title', t('switchTheme'));
    $('btnLang').setAttribute('title', t('switchLanguage'));
    $('paletteFoot').innerHTML = `<span><kbd>↑↓</kbd> ${esc(t('navigate'))}</span><span><kbd>↵</kbd> ${esc(t('open'))}</span><span><kbd>Esc</kbd> ${esc(t('closeShort'))}</span>`;
    setTheme(state.theme);
    if (state.currentView === 'explorer') renderExplorer();
    else if (state.currentView === 'share') renderShare();
    else if (state.currentView === 'admin') loadAdminPanel();
    renderSidebar();
  }
  function setView(view) {
    state.view = view === 'grid' ? 'grid' : 'list';
    writeStore('kova.view', state.view);
    $('btnViewList').classList.toggle('is-active', state.view === 'list');
    $('btnViewGrid').classList.toggle('is-active', state.view === 'grid');
    $('btnViewList').setAttribute('aria-pressed', String(state.view === 'list'));
    $('btnViewGrid').setAttribute('aria-pressed', String(state.view === 'grid'));
    renderExplorer();
  }
  function updateAuthButton() {
    $('btnAuth').textContent = state.user ? state.user.username : t('signIn');
    $('btnTopAdmin').hidden = !state.user || Number(state.user.role) !== 2;
    $('writeToolbar').hidden = !state.canWrite;
  }
  function closeSidebar() {
    $('appSidebar').classList.remove('is-open');
    $('sidebarScrim').hidden = true;
    $('btnMobileNav').setAttribute('aria-expanded', 'false');
  }
  function setViewMode(view) {
    state.currentView = view;
    $('explorerView').hidden = view !== 'explorer';
    $('adminView').hidden = view !== 'admin';
    $('shareView').hidden = view !== 'share';
    $('appShell').classList.toggle('is-share-mode', view === 'share');
    if (view !== 'admin') stopAdminTimer();
    closeSidebar();
  }
  function updateFolderSummary() {
    const sum = $('folderSummary');
    if (!sum) return;
    const bytes = state.filtered.reduce((n, item) => n + (Number(item.size) || 0), 0);
    sum.textContent = `${t('itemCount', { n: state.filtered.length })} · ${t('bytesUsed', { size: formatBytes(bytes) })}`;
  }
  function fileIcon(item) {
    const type = Number(item.type) || (item.is_dir ? 1 : 0);
    if (item.is_dir || type === 1) return '<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="#f3b743" d="M2.5 6.5A2.5 2.5 0 0 1 5 4h5l2 2h7.5A2.5 2.5 0 0 1 22 8.5v9a2.5 2.5 0 0 1-2.5 2.5h-14A3 3 0 0 1 2.5 17z"/><path fill="#ffd36f" d="M2.5 9h19v8.5a2.5 2.5 0 0 1-2.5 2.5h-14A2.5 2.5 0 0 1 2.5 17z"/></svg>';
    const colors = { 2: ['#e6534b', '#ffe3df'], 3: ['#2674d8', '#d9eaff'], 4: ['#0c8e81', '#d7f3ed'], 5: ['#1d6fba', '#d9edff'], 6: ['#13856d', '#d8f3e9'], 7: ['#d48a1b', '#fff0cc'], 0: ['#687687', '#e8edf2'] };
    const [color, soft] = colors[type] || colors[0];
    const glyph = type === 3 ? '<path fill="' + color + '" d="m9 7 8 5-8 5z"/>'
      : type === 4 ? '<path fill="none" stroke="' + color + '" stroke-width="1.7" d="M5 13a2 2 0 0 1 0-4 3.5 3.5 0 0 1 6.7-.7A2.4 2.4 0 0 1 17 10a2 2 0 0 1 0 4H5z"/><path fill="' + color + '" d="M10 10v5l3-2.5z"/>'
      : type === 6 ? '<rect x="6" y="6" width="12" height="12" rx="2" fill="' + soft + '"/><circle cx="9.5" cy="10" r="1.5" fill="' + color + '"/><path fill="' + color + '" d="m7 17 3.5-4 2.5 2 2-2.5 2 4.5z"/>'
      : type === 7 ? '<path fill="' + color + '" d="M9 7h6l4 4v6H5V7z"/><path fill="' + soft + '" d="M15 7v4h4"/><path fill="none" stroke="' + soft + '" stroke-width="1.4" d="M8 14h8"/>'
      : type === 5 ? '<path fill="none" stroke="' + color + '" stroke-width="1.8" d="m9 9-3 3 3 3m6-6 3 3-3 3m-2-7-2 8"/>'
      : '<path fill="' + soft + '" d="M6 4h8l4 4v12H6z"/><path fill="' + color + '" d="M14 4v5h5"/><path fill="none" stroke="' + color + '" stroke-width="1.4" d="M8.5 13h7M8.5 16h5"/>';
    return `<svg viewBox="0 0 24 24" aria-hidden="true"><path fill="${soft}" d="M5 2.8h9l5 5V21H5z"/><path fill="${color}" d="M14 2.8v5h5"/>${glyph}</svg>`;
  }
  const DRIVER_SVGS = {
    Local: '<svg viewBox="0 0 32 32" aria-hidden="true"><rect x="4" y="6" width="24" height="20" rx="4" fill="#e6ebf0"/><rect x="7" y="9" width="18" height="10" rx="2" fill="#8997a7"/><circle cx="10" cy="23" r="1.5" fill="#28a36a"/><path d="M14 23h10" stroke="#8190a1" stroke-width="2" stroke-linecap="round"/></svg>',
    S3: '<svg viewBox="0 0 32 32" aria-hidden="true"><path fill="#f2a12d" d="M7 6h18l-2 21H9z"/><path fill="#ffcf6a" d="M9 11h14l-1.3 12H10.3z"/><path fill="#9b5912" d="M12 15h8v2h-8zm0 4h8v2h-8z"/></svg>',
    WebDAV: '<svg viewBox="0 0 32 32" aria-hidden="true"><path fill="#1673bd" d="M9 24a6 6 0 0 1-.7-12A8 8 0 0 1 23.8 13 5.5 5.5 0 0 1 24 24z"/><path fill="#4da8db" d="M11 20a4 4 0 0 1 0-8 5 5 0 0 1 9-1 4 4 0 0 1 1 8z"/><path fill="#fff" d="m12 16 2.1 2.2 4.4-4.7" stroke="#fff" stroke-width="1.5"/></svg>',
    GoogleDrive: '<svg viewBox="0 0 32 32" aria-hidden="true"><path fill="#00875a" d="M11 5h7l10 17h-7z"/><path fill="#4285f4" d="M4 22h24l-4 6H8z"/><path fill="#ffba00" d="M11 5h7L8 28H2z"/><path fill="#00a66c" d="M8 22h7l-3.5 6H5z"/><path fill="#ea4335" d="M21 22h7l-4 6h-7z"/></svg>',
    OneDrive: '<svg viewBox="0 0 32 32" aria-hidden="true"><path fill="#1473c9" d="M12.1 23H6a4.7 4.7 0 0 1-.4-9.4A7.4 7.4 0 0 1 19.4 12 5.4 5.4 0 0 1 23 22z"/><path fill="#36a5f5" d="M16 23h-2.5a4.1 4.1 0 0 1-.3-8.2 6.3 6.3 0 0 1 11.4 0 4 4 0 0 1 .4 8.2z"/></svg>',
    Dropbox: '<svg viewBox="0 0 32 32" aria-hidden="true"><path fill="#0061ff" d="m10 6 7 4.5L10 15l-7-4.5zM22 6l7 4.5-7 4.5-7-4.5zM10 16l7 4.5L10 25l-7-4.5zM22 16l7 4.5-7 4.5-7-4.5z"/><path fill="#4c9aff" d="m16 23.5 6-4 6 4-6 4z"/></svg>',
    SFTP: '<svg viewBox="0 0 32 32" aria-hidden="true"><rect x="5" y="5" width="22" height="9" rx="3" fill="#2071b9"/><rect x="5" y="18" width="22" height="9" rx="3" fill="#36a0c9"/><circle cx="10" cy="9.5" r="1.3" fill="#b9ebff"/><circle cx="10" cy="22.5" r="1.3" fill="#d6f7ff"/><path d="M15 9.5h8M15 22.5h8" stroke="#fff" stroke-width="1.5" stroke-linecap="round"/></svg>',
    FTP: '<svg viewBox="0 0 32 32" aria-hidden="true"><path fill="#3476a8" d="M5 7a3 3 0 0 1 3-3h16a3 3 0 0 1 3 3v5H5z"/><path fill="#65a4ca" d="M5 14h22v5H5z"/><path fill="#3476a8" d="M5 21h22v4a3 3 0 0 1-3 3H8a3 3 0 0 1-3-3z"/><circle cx="10" cy="9" r="1.3" fill="#f7cf63"/><circle cx="10" cy="16.5" r="1.3" fill="#dff5ff"/><circle cx="10" cy="23.5" r="1.3" fill="#8ce1a7"/></svg>',
    URLTree: '<svg viewBox="0 0 32 32" aria-hidden="true"><path fill="#28a36a" d="M14 6h5v5h-5zM5 21h5v5H5zM22 21h5v5h-5z"/><path d="M16.5 11v5M7.5 16h17M7.5 16v5M24.5 16v5" stroke="#367d60" stroke-width="2" stroke-linecap="round"/></svg>',
    Alias: '<svg viewBox="0 0 32 32" aria-hidden="true"><rect x="4" y="7" width="15" height="17" rx="3" fill="#5595d4"/><rect x="13" y="10" width="15" height="17" rx="3" fill="#f0b64a"/><path d="M13 15h6" stroke="#fff" stroke-width="2"/></svg>',
    Crypt: '<svg viewBox="0 0 32 32" aria-hidden="true"><rect x="5" y="13" width="22" height="15" rx="3" fill="#167e85"/><path d="M10 13V9a6 6 0 0 1 12 0v4" fill="none" stroke="#49c2ad" stroke-width="3"/><circle cx="16" cy="20" r="2" fill="#d6f6eb"/><path d="M16 22v2" stroke="#d6f6eb" stroke-width="2"/></svg>'
  };
  function driverTitle(driver) {
    const keys = { Local: 'driverLocal', S3: 'driverS3', WebDAV: 'driverWebDAV', GoogleDrive: 'driverGoogleDrive', OneDrive: 'driverOneDrive', Dropbox: 'driverDropbox', SFTP: 'driverSFTP', FTP: 'driverFTP', URLTree: 'driverURLTree', Alias: 'driverAlias', Crypt: 'driverCrypt' };
    return t(keys[driver] || 'driver');
  }
  function driverIcon(driver) { return `<span class="driver-icon brand-svg" aria-hidden="true">${DRIVER_SVGS[driver] || DRIVER_SVGS.Local}</span>`; }
  function renderSidebar() {
    const mounts = state.rootItems.filter(item => item.is_dir && item.name !== '..').slice(0, 30);
    const active = path => pathJoin('/', path) === pathJoin('/', state.path);
    const nav = (label, path, glyph, selected = false, driver = '') => `<button type="button" class="nav-item${selected ? ' is-active' : ''}" data-act="navigate" data-path="${esc(path)}" aria-current="${selected ? 'page' : 'false'}">${driver ? driverIcon(driver) : glyph}<span class="nav-item-label">${esc(label)}</span></button>`;
    $('sidebarContent').innerHTML = `
      <div class="nav-group"><div class="nav-group-label">${esc(t('navigation'))}</div>
        ${nav(t('homeNav'), '/', '<svg viewBox="0 0 20 20" aria-hidden="true"><path d="m3 9 7-6 7 6v7a1 1 0 0 1-1 1h-4v-5H8v5H4a1 1 0 0 1-1-1z"/></svg>', active('/'))}
        ${mounts.map(item => nav(item.name, pathJoin('/', item.name), '<svg viewBox="0 0 20 20" aria-hidden="true"><path d="M2.8 6a1.5 1.5 0 0 1 1.5-1.5h4l1.5 1.7h6A1.4 1.4 0 0 1 17.2 7.6v7a1.5 1.5 0 0 1-1.5 1.5H4.3a1.5 1.5 0 0 1-1.5-1.5z"/></svg>', active(pathJoin('/', item.name)))).join('')}
      </div>
      <div class="nav-group"><div class="nav-group-label">${esc(t('pinned'))}</div>
        ${nav(t('allFiles'), '/', '<svg viewBox="0 0 20 20" aria-hidden="true"><path d="M3.5 5.5h13v10h-13zM6.5 8.5h7M6.5 11.5h5"/></svg>', active('/'))}
        <button type="button" class="nav-item" data-act="open-account"><svg viewBox="0 0 20 20" aria-hidden="true"><circle cx="10" cy="6.5" r="3"/><path d="M4 17a6 6 0 0 1 12 0"/></svg><span class="nav-item-label">${esc(t('account'))}</span></button>
        <button type="button" class="nav-item" data-act="show-shortcuts"><svg viewBox="0 0 20 20" aria-hidden="true"><rect x="2.8" y="4" width="14.4" height="12" rx="2"/><path d="M6 8h.01M9 8h.01M12 8h.01M15 8h.01M6 12h.01M9 12h.01M12 12h.01"/></svg><span class="nav-item-label">${esc(t('shortcuts'))}</span></button>
      </div>`;
    $('sidebarFooter').innerHTML = state.user ? `<div class="user-chip"><div class="user-avatar" aria-hidden="true">${esc((state.user.username || 'K').slice(0, 1).toUpperCase())}</div><div class="user-meta"><div class="user-name">${esc(state.user.username)}</div><div class="user-role">${esc(Number(state.user.role) === 2 ? t('administrator') : Number(state.user.role) === 1 ? t('guest') : t('standardUser'))}</div></div><button type="button" class="btn btn--ghost icon-btn icon-btn--sm" data-act="open-account" aria-label="${esc(t('account'))}"><svg viewBox="0 0 20 20" aria-hidden="true"><path d="M10 3.5v13M3.5 10h13"/></svg></button></div>
      <button type="button" class="btn btn--ghost" data-act="sign-out">${esc(t('signOut'))}</button>`
      : `<div class="user-chip"><div class="user-avatar" aria-hidden="true">K</div><div class="user-meta"><div class="user-name">Kova</div><div class="user-role">${esc(t('allFiles'))}</div></div></div>`;
    $('btnTopAdmin').hidden = !state.user || Number(state.user.role) !== 2;
  }

  function applySort(items) {
    const collator = new Intl.Collator(state.lang === 'vi' ? 'vi' : undefined, { numeric: true, sensitivity: 'base' });
    const { key, dir } = state.sort;
    return [...items].sort((a, b) => {
      if (!!a.is_dir !== !!b.is_dir) return a.is_dir ? -1 : 1;
      let cmp = 0;
      if (key === 'size') cmp = (Number(a.size) || 0) - (Number(b.size) || 0);
      else if (key === 'modified') cmp = new Date(a.modified || 0).getTime() - new Date(b.modified || 0).getTime();
      else cmp = collator.compare(a.name || '', b.name || '');
      return cmp * dir;
    });
  }
  // Listings are replaced on every fetch, never mutated in place. Reuse the
  // derived order when selection, layout or progressive rendering changes.
  let filteredCache = null;
  function filteredItems() {
    const needle = normalizedSearch(state.filter.trim());
    const { key, dir } = state.sort;
    if (filteredCache && filteredCache.items === state.items && filteredCache.needle === needle &&
        filteredCache.key === key && filteredCache.dir === dir && filteredCache.lang === state.lang) {
      return filteredCache.result;
    }
    const list = needle ? state.items.filter(item => normalizedSearch(item.name).includes(needle)) : state.items;
    const result = applySort(list);
    filteredCache = { items: state.items, needle, key, dir, lang: state.lang, result };
    return result;
  }
  function makeCrumbs(container, path, share = false) {
    const segments = pathJoin('/', path).split('/').filter(Boolean);
    let current = '/';
    const parts = [{ name: share ? t('sharedLink') : t('homeNav'), path: '/' }];
    for (const segment of segments) { current = pathJoin(current, segment); parts.push({ name: segment, path: current }); }
    container.innerHTML = parts.map((part, index) => `${index ? '<span class="crumb-sep" aria-hidden="true">/</span>' : ''}<button type="button" class="crumb-btn${index === parts.length - 1 ? ' is-current' : ''}" data-act="${share ? 'share-navigate' : 'navigate'}" data-path="${esc(part.path)}" ${index === parts.length - 1 ? 'aria-current="page"' : ''}>${esc(part.name)}</button>`).join('');
  }
  function renderSkeleton() {
    $('explorerContent').innerHTML = `<div class="skeleton-list" aria-label="${esc(t('loading'))}">${'<div class="skeleton-row"></div>'.repeat(6)}</div>`;
  }
  function stateBox(title, desc, action = '') {
    return `<div class="state-box"><div class="state-icon"><svg viewBox="0 0 20 20" aria-hidden="true"><path d="M3 5.5h14v10H3zM6 8.5h8M6 11.5h5"/></svg></div><h2 class="state-title">${esc(title)}</h2><p class="state-desc">${esc(desc)}</p>${action ? `<div class="state-actions">${action}</div>` : ''}</div>`;
  }
  function itemFullPath(item) { return pathJoin(state.path, item.name); }
  function itemRow(item, index, grid = false) {
    const path = itemFullPath(item);
    const selected = state.selected.has(path);
    const time = formatDate(item.modified);
    const size = item.is_dir ? '—' : formatBytes(item.size);
    const sub = item.is_dir ? t('folder') : (item.type ? `${t('type')}: ${item.type}` : t('file'));
    if (grid) return `<article class="grid-card${selected ? ' is-selected' : ''}" data-item-path="${esc(path)}" data-index="${index}" tabindex="0" data-act="item-open" aria-label="${esc(item.name)}">
      <div class="grid-card-top"><span class="file-icon">${fileIcon(item)}</span><input class="check-box" type="checkbox" data-act="toggle-select" data-path="${esc(path)}" aria-label="${esc(`${t('selectAll')} ${item.name}`)}" ${selected ? 'checked' : ''}></div>
      <div class="grid-card-name">${esc(item.name)}</div><div class="grid-card-meta"><span>${esc(size)}</span><span>${esc(item.is_dir ? t('folder') : time)}</span></div></article>`;
    return `<tr class="file-row${selected ? ' is-selected' : ''}${state.focusIndex === index ? ' is-focused' : ''}" data-item-path="${esc(path)}" data-index="${index}" tabindex="-1" data-act="item-open">
      <td class="col-check"><input class="check-box" type="checkbox" data-act="toggle-select" data-path="${esc(path)}" aria-label="${esc(`${t('selectAll')} ${item.name}`)}" ${selected ? 'checked' : ''}></td>
      <td><div class="file-cell"><span class="file-icon">${fileIcon(item)}</span><span class="file-name-wrap"><span class="file-name">${esc(item.name)}</span><span class="file-sub">${esc(sub)}</span></span></div></td>
      <td class="col-size">${esc(size)}</td><td class="col-date">${esc(time)}</td>
      <td class="col-ops"><span class="row-actions"><button type="button" class="btn btn--ghost icon-btn icon-btn--sm hide-sm" data-act="item-download" data-path="${esc(path)}" aria-label="${esc(`${t('download')} ${item.name}`)}" title="${esc(t('download'))}"><svg viewBox="0 0 20 20" aria-hidden="true"><path d="M10 3.5v9M6.5 9.5 10 13l3.5-3.5M4 15v1.5h12V15"/></svg></button><button type="button" class="btn btn--ghost icon-btn icon-btn--sm" data-act="item-menu" data-path="${esc(path)}" aria-label="${esc(`${t('actions')} ${item.name}`)}"><svg viewBox="0 0 20 20" aria-hidden="true"><circle cx="4.5" cy="10" r=".8"/><circle cx="10" cy="10" r=".8"/><circle cx="15.5" cy="10" r=".8"/></svg></button></span></td></tr>`;
  }
  function renderExplorer() {
    if (!$('explorerContent')) return;
    makeCrumbs($('breadcrumbs'), state.path);
    $('filterInput').value = state.filter;
    state.filtered = filteredItems();
    updateFolderSummary();
    $('writeToolbar').hidden = !state.canWrite;
    $('readmeCard').hidden = !Boolean(state.listMeta?.readme);
    if (state.listMeta?.readme) {
      $('readmeHeading').textContent = state.path === '/' ? t('folderNotes') : `${t('folderNotes')}: ${state.path.split('/').filter(Boolean).at(-1) || ''}`;
      $('btnReadmeMode').textContent = state.readmeRaw ? t('rendered') : t('raw');
      $('readmeBody').innerHTML = state.readmeRaw ? `<pre class="code-view is-wrap">${esc(state.listMeta.readme)}</pre>` : renderMarkdown(state.listMeta.readme);
    }
    if (!state.items.length) {
      $('explorerContent').innerHTML = state.canWrite
        ? stateBox(t('noItems'), t('noItemsDesc'), `<button type="button" class="btn btn--primary" data-act="trigger-upload">${esc(t('upload'))}</button><button type="button" class="btn" data-act="new-folder">${esc(t('newFolder'))}</button>`)
        : stateBox(t('noItems'), t('noItemsDesc'));
      renderSidebar(); updateBatchDock(); return;
    }
    if (!state.filtered.length) {
      $('explorerContent').innerHTML = stateBox(t('emptySearch'), t('noResults'));
      updateBatchDock(); return;
    }
    const visible = state.filtered.slice(0, state.renderLimit);
    if (state.view === 'grid') {
      $('explorerContent').innerHTML = `<div class="file-grid">${visible.map((item, index) => itemRow(item, index, true)).join('')}</div>${visible.length < state.filtered.length ? `<div class="load-more-row"><button type="button" class="btn" data-act="load-more">${esc(t('loading'))} · ${visible.length}/${state.filtered.length}</button></div>` : ''}`;
    } else {
      const sortAria = key => state.sort.key === key ? ` aria-sort="${state.sort.dir > 0 ? 'ascending' : 'descending'}"` : '';
      const sorted = key => `<button type="button" class="sort-btn${state.sort.key === key ? ' is-sorted' : ''}" data-act="sort" data-sort="${key}" aria-label="${esc(t(key === 'name' ? 'sortName' : key === 'size' ? 'sortSize' : 'sortDate'))}">${esc(t(key === 'name' ? 'name' : key === 'size' ? 'size' : 'modified'))}${state.sort.key === key ? (state.sort.dir > 0 ? ' ↑' : ' ↓') : ''}</button>`;
      $('explorerContent').innerHTML = `<div class="table-wrap"><table class="file-table"><thead><tr><th class="col-check"><span class="sr-only">${esc(t('selectAll'))}</span></th><th${sortAria('name')}>${sorted('name')}</th><th class="col-size"${sortAria('size')}>${sorted('size')}</th><th class="col-date"${sortAria('modified')}>${sorted('modified')}</th><th class="col-ops">${esc(t('actions'))}</th></tr></thead><tbody>${visible.map((item, index) => itemRow(item, index)).join('')}</tbody></table></div>${visible.length < state.filtered.length ? `<div class="load-more-row"><button type="button" class="btn" data-act="load-more">${esc(t('loading'))} · ${visible.length}/${state.filtered.length}</button></div>` : ''}`;
    }
    observeLoadMore(); updateBatchDock(); renderSidebar();
  }
  let listObserver = null;
  function observeLoadMore() {
    listObserver?.disconnect();
    if (state.renderLimit >= state.filtered.length) return;
    const marker = document.querySelector('.load-more-row');
    if (!marker) return;
    if ('IntersectionObserver' in window) {
      listObserver = new IntersectionObserver(entries => {
        if (entries.some(entry => entry.isIntersecting)) {
          state.renderLimit = Math.min(state.filtered.length, state.renderLimit + 160);
          renderExplorer();
        }
      }, { rootMargin: '500px' });
      listObserver.observe(marker);
    }
  }
  async function loadDir(path = state.path, refresh = false, passwordRetry = false) {
    const nextPath = pathJoin('/', path);
    state.listController?.abort();
    state.listController = new AbortController();
    const controller = state.listController;
    state.path = nextPath;
    state.selected.clear(); state.focusIndex = -1; state.renderLimit = 160;
    setViewMode('explorer');
    renderSkeleton(); makeCrumbs($('breadcrumbs'), nextPath);
    try {
      const data = await api('/api/fs/list', { body: { path: nextPath, refresh, per_page: 0, password: state.pathPasswords[nextPath] || '' }, signal: controller.signal });
      if (controller.signal.aborted) return;
      state.items = safeArray(data?.content);
      if (nextPath === '/') state.rootItems = state.items;
      state.listMeta = data || {};
      state.canWrite = Boolean(data?.write);
      state.filter = $('filterInput').value || '';
      state.readmeRaw = false;
      renderExplorer(); updateAuthButton();
    } catch (error) {
      if (error.name === 'AbortError') return;
      state.items = []; state.listMeta = null; state.canWrite = false;
      renderExplorer();
      if (error.code === 401 && !state.user) { openLogin(error.message); return; }
      if ((error.code === 401 || error.code === 403) && !passwordRetry) { openPasswordGate(nextPath, error.message); return; }
      toast(error.message || t('operationFailed'), 'err');
    }
  }
  function renderBatchDock() {
    const count = state.selected.size;
    $('batchDock').hidden = count === 0;
    $('batchCount').textContent = t('selected', { n: count });
  }
  function updateBatchDock() { renderBatchDock(); }
  function setFocused(index) {
    const max = state.filtered.length - 1;
    if (max < 0) return;
    state.focusIndex = Math.max(0, Math.min(max, index));
    if (state.focusIndex >= state.renderLimit) { state.renderLimit = Math.min(state.filtered.length, state.focusIndex + 40); renderExplorer(); }
    const element = document.querySelector(`[data-index="${state.focusIndex}"][data-item-path]`);
    element?.scrollIntoView({ block: 'nearest' }); element?.focus({ preventScroll: true });
  }
  function selectionFromKey(path, additive = false) {
    if (!additive) state.selected.clear();
    if (state.selected.has(path)) state.selected.delete(path); else state.selected.add(path);
    renderExplorer();
  }
  async function openItem(path, share = false) {
    const target = pathJoin('/', path);
    if (share) {
      const item = state.shareItems.find(entry => pathJoin(state.shareSubpath, entry.name) === target) || { name: target.split('/').at(-1), is_dir: target === state.shareSubpath || target === '/' };
      if (item.is_dir) { state.shareSubpath = target; await loadShareDir(target); return; }
      await openPreview(target, true, item); return;
    }
    const item = state.items.find(entry => itemFullPath(entry) === target);
    if (!item) { await openPreview(target, false); return; }
    if (item.is_dir) { await loadDir(target); return; }
    if (Number(item.type) === 7 || /\.(zip)$/i.test(item.name)) { await openArchive(target); return; }
    await openPreview(target, false, item);
  }

  function renderMarkdown(source) {
    const text = String(source || '').replace(/\r\n?/g, '\n');
    const placeholders = [];
    const inline = value => {
      let safe = value.replace(/\[([^\]]{1,300})\]\(([^)\s]{1,1500})(?:\s+"[^"]*")?\)/g, (whole, label, href) => {
        let url;
        try { url = new URL(href, location.origin); } catch { return esc(label); }
        if (!['http:', 'https:', 'mailto:'].includes(url.protocol)) return esc(label);
        const rendered = `<a href="${esc(url.href)}" target="_blank" rel="noopener noreferrer">${esc(label)}</a>`;
        const token = `\u0000${placeholders.length}\u0000`;
        placeholders.push(rendered); return token;
      });
      safe = esc(safe);
      safe = safe.replace(/`([^`]+)`/g, '<code>$1</code>')
        .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
        .replace(/\*([^*]+)\*/g, '<em>$1</em>');
      safe = safe.replace(/\u0000(\d+)\u0000/g, (_, i) => placeholders[Number(i)] || '');
      return safe;
    };
    const lines = text.split('\n');
    const out = []; let paragraph = []; let list = ''; let code = false; let codeLines = [];
    const flushParagraph = () => { if (paragraph.length) { out.push(`<p>${paragraph.map(inline).join('<br>')}</p>`); paragraph = []; } };
    const flushList = () => { if (list) { out.push(`</${list}>`); list = ''; } };
    for (const line of lines) {
      if (/^\s*```/.test(line)) { flushParagraph(); flushList(); if (code) { out.push(`<pre><code>${esc(codeLines.join('\n'))}</code></pre>`); codeLines = []; code = false; } else code = true; continue; }
      if (code) { codeLines.push(line); continue; }
      if (!line.trim()) { flushParagraph(); flushList(); continue; }
      const heading = line.match(/^\s*(#{1,4})\s+(.+)$/);
      if (heading) { flushParagraph(); flushList(); const n = heading[1].length; out.push(`<h${n}>${inline(heading[2])}</h${n}>`); continue; }
      const li = line.match(/^\s*[-*+]\s+(.+)$/);
      if (li) { flushParagraph(); if (list !== 'ul') { flushList(); list = 'ul'; out.push('<ul>'); } out.push(`<li>${inline(li[1])}</li>`); continue; }
      const oi = line.match(/^\s*\d+\.\s+(.+)$/);
      if (oi) { flushParagraph(); if (list !== 'ol') { flushList(); list = 'ol'; out.push('<ol>'); } out.push(`<li>${inline(oi[1])}</li>`); continue; }
      if (/^\s*&gt;/.test(esc(line))) { flushParagraph(); flushList(); out.push(`<blockquote><p>${inline(line.replace(/^\s*>\s?/, ''))}</p></blockquote>`); continue; }
      flushList(); paragraph.push(line);
    }
    if (code) out.push(`<pre><code>${esc(codeLines.join('\n'))}</code></pre>`);
    flushParagraph(); flushList();
    return out.join('');
  }
  function openModal(html, { wide = false, xl = false, focus = true } = {}) {
    state.modalReturnFocus = document.activeElement;
    $('modalDialog').classList.toggle('is-wide', wide || xl);
    $('modalDialog').classList.toggle('is-xl', xl);
    $('modalContent').innerHTML = html;
    $('modalOverlay').hidden = false;
    document.body.classList.add('has-overlay');
    if (focus) setTimeout(() => $('modalContent').querySelector('[autofocus], input, select, textarea, button')?.focus(), 0);
  }
  function closeModal() {
    $('modalOverlay').hidden = true;
    $('modalContent').innerHTML = '';
    state.picker = null;
    $('modalDialog').classList.remove('is-wide', 'is-xl');
    for (const url of state.previewUrls) URL.revokeObjectURL(url);
    state.previewUrls = [];
    document.body.classList.toggle('has-overlay', !$('paletteOverlay').hidden);
    state.modalReturnFocus?.focus?.();
    state.modalReturnFocus = null;
  }
  function modalHead(title, subtitle = '') {
    return `<div class="modal-head"><div><h2 class="modal-title" id="modalTitle">${esc(title)}</h2>${subtitle ? `<p class="modal-sub">${esc(subtitle)}</p>` : ''}</div><button type="button" class="btn btn--ghost icon-btn icon-btn--sm" data-act="close-modal" aria-label="${esc(t('close'))}"><svg viewBox="0 0 20 20" aria-hidden="true"><path d="m5.5 5.5 9 9m0-9-9 9"/></svg></button></div>`;
  }
  function field(name, label, value = '', type = 'text', options = {}) {
    const id = options.id || `f-${name.replace(/[^a-z0-9_-]/gi, '-')}`;
    const attrs = `${options.required ? ' required' : ''}${options.min !== undefined ? ` min="${esc(options.min)}"` : ''}${options.max !== undefined ? ` max="${esc(options.max)}"` : ''}${options.step !== undefined ? ` step="${esc(options.step)}"` : ''}${options.placeholder ? ` placeholder="${esc(options.placeholder)}"` : ''}${options.autocomplete ? ` autocomplete="${esc(options.autocomplete)}"` : ''}${options.autofocus ? ' autofocus' : ''}`;
    if (type === 'textarea') return `<label class="field" for="${esc(id)}"><span class="field-label">${esc(label)}</span><textarea class="input" id="${esc(id)}" name="${esc(name)}"${attrs}>${esc(value)}</textarea></label>`;
    return `<label class="field" for="${esc(id)}"><span class="field-label">${esc(label)}</span><input class="input" id="${esc(id)}" name="${esc(name)}" type="${esc(type)}" value="${esc(value)}"${attrs}></label>`;
  }
  function checkbox(name, label, checked = false, extra = '') {
    return `<label class="check-row"><input type="checkbox" name="${esc(name)}" ${checked ? 'checked' : ''} ${extra}><span>${esc(label)}</span></label>`;
  }
  function closePalette() {
    $('paletteOverlay').hidden = true;
    document.body.classList.toggle('has-overlay', !$('modalOverlay').hidden);
    state.searchController?.abort(); hideContext(); $('cmdTrigger').focus();
  }
  function openPalette() {
    closeSidebar(); $('paletteOverlay').hidden = false; document.body.classList.add('has-overlay');
    $('paletteInput').value = ''; state.palette.index = 0; state.palette.results = [];
    renderPalette([]); setTimeout(() => $('paletteInput').focus(), 0);
  }
  function renderPalette(results) {
    const commands = [
      { title: t('upload'), sub: t('fileUpload'), act: 'command', key: 'upload' },
      { title: t('newFolder'), sub: t('createFolderTitle'), act: 'command', key: 'mkdir' },
      { title: t('newFile'), sub: t('createFileTitle'), act: 'command', key: 'newfile' },
      { title: t('fetchUrl'), sub: t('offlineTitle'), act: 'command', key: 'offline' },
      { title: t('account'), sub: t('accountTitle'), act: 'command', key: 'account' },
      ...(state.user && Number(state.user.role) === 2 ? [{ title: t('console'), sub: t('admin'), act: 'command', key: 'admin' }] : [])
    ];
    const query = normalizedSearch($('paletteInput').value.trim());
    const matchingCommands = query ? commands.filter(item => normalizedSearch(`${item.title} ${item.sub}`).includes(query)) : commands;
    const items = [...matchingCommands.map(item => ({ ...item, kind: 'command' })), ...results.map(item => ({ ...item, kind: 'file' }))];
    state.palette.results = items;
    state.palette.index = Math.max(0, Math.min(state.palette.index, items.length - 1));
    $('paletteResults').innerHTML = items.length ? items.map((item, index) => `<button type="button" role="option" aria-selected="${index === state.palette.index}" class="palette-item${index === state.palette.index ? ' is-active' : ''}" data-act="palette-result" data-index="${index}"><span class="file-icon">${item.kind === 'command' ? '<svg viewBox="0 0 20 20" aria-hidden="true"><path d="m8 5-5 5 5 5m4-10 5 5-5 5"/></svg>' : fileIcon(item)}</span><span class="palette-item-main"><span class="palette-item-title">${esc(item.title || item.name)}</span><span class="palette-item-sub">${esc(item.sub || item.parent || '')}</span></span><span class="palette-item-meta">${esc(item.kind === 'command' ? '↵' : item.is_dir ? t('folder') : formatBytes(item.size))}</span></button>`).join('') : `<div class="state-box"><p class="state-desc">${esc(query ? t('noResults') : t('searchPlaceholder'))}</p></div>`;
  }
  async function searchPalette(query) {
    state.searchController?.abort();
    const controller = new AbortController(); state.searchController = controller;
    const q = query.trim();
    if (!q) { renderPalette([]); return; }
    try {
      const data = await api('/api/fs/search', { body: { parent: state.palette.global ? '/' : state.path, keywords: q, scope: state.palette.scope, page: 1, per_page: 60 }, signal: controller.signal });
      if (controller.signal.aborted) return;
      const hits = safeArray(data?.content).map(item => ({ ...item, title: item.name, sub: item.parent || '/', path: pathJoin(item.parent || '/', item.name) }));
      renderPalette(hits);
    } catch (error) { if (error.name !== 'AbortError') { renderPalette([]); toast(error.message, 'err'); } }
  }
  function schedulePaletteSearch() {
    clearTimeout(state.palette.timer);
    state.palette.timer = setTimeout(() => searchPalette($('paletteInput').value), 180);
  }
  function runCommand(key) {
    closePalette();
    if (key === 'upload') $('fileInput').click();
    else if (key === 'mkdir') openCreateFolder();
    else if (key === 'newfile') openCreateFile();
    else if (key === 'offline') openOfflineDialog();
    else if (key === 'account') openAccount();
    else if (key === 'admin') openAdmin();
  }
  async function choosePaletteResult(index) {
    const item = state.palette.results[index]; if (!item) return;
    if (item.kind === 'command') { runCommand(item.key); return; }
    closePalette();
    state.path = pathJoin('/', item.parent || '/');
    await loadDir(state.path);
    await openItem(item.path, false);
  }
  function openLogin(message = '') {
    const value = `<form data-form="login">${modalHead(t('signInTitle'), t('signInDesc'))}${message ? `<p class="field-help">${esc(message)}</p>` : ''}
      ${field('username', t('username'), '', 'text', { required: true, autocomplete: 'username' })}${field('password', t('password'), '', 'password', { required: true, autocomplete: 'current-password' })}
      ${field('otp_code', t('otpCode'), '', 'text', { autocomplete: 'one-time-code', placeholder: t('otpPlaceholder') })}
      <div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="submit" class="btn btn--primary">${esc(t('submitSignIn'))}</button></div></form>`;
    openModal(value);
  }
  function openPasswordGate(path, message = '') {
    openModal(`<form data-form="password-gate" data-path="${esc(path)}">${modalHead(t('passwordGate'), t('passwordGateDesc'))}${message ? `<p class="field-help">${esc(message)}</p>` : ''}${field('password', t('password'), '', 'password', { required: true, autocomplete: 'current-password' })}<div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="submit" class="btn btn--primary">${esc(t('unlock'))}</button></div></form>`);
  }
  function openCreateFolder() {
    if (!state.canWrite) { toast(t('noWrite'), 'err'); return; }
    openModal(`<form data-form="mkdir">${modalHead(t('createFolderTitle'))}${field('name', t('folderName'), '', 'text', { required: true, autofocus: true })}<div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="submit" class="btn btn--primary">${esc(t('create'))}</button></div></form>`);
  }
  function openCreateFile() {
    if (!state.canWrite) { toast(t('noWrite'), 'err'); return; }
    openModal(`<form data-form="new-file">${modalHead(t('createFileTitle'))}${field('name', t('fileName'), 'untitled.txt', 'text', { required: true, autofocus: true })}${field('content', t('content'), '', 'textarea')}<div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="submit" class="btn btn--primary">${esc(t('create'))}</button></div></form>`);
  }
  function openOfflineDialog() {
    openModal(`<form data-form="offline">${modalHead(t('offlineTitle'))}${field('urls', t('urls'), '', 'textarea', { required: true, placeholder: 'https://example.com/file.zip' })}<div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="submit" class="btn btn--primary">${esc(t('addToQueue'))}</button></div></form>`);
  }
  function openRename(path) {
    const item = state.items.find(entry => itemFullPath(entry) === path);
    if (!item) return;
    openModal(`<form data-form="rename" data-path="${esc(path)}">${modalHead(t('renameTitle'))}${field('name', t('newName'), item.name, 'text', { required: true, autofocus: true })}<div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="submit" class="btn btn--primary">${esc(t('save'))}</button></div></form>`);
  }
  function openDeleteConfirm(paths) {
    state.pendingDelete = paths;
    openModal(`${modalHead(t('confirmDelete'), t('confirmDeleteDesc'))}<div class="pill">${esc(t('selected', { n: paths.length }))}</div><div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="button" class="btn btn--danger" data-act="confirm-delete">${esc(t('deleteNow'))}</button></div>`);
  }
  function openShareDialog(path) {
    openModal(`<form data-form="share-create" data-path="${esc(path)}">${modalHead(t('shareTitle'))}${field('password', t('sharePassword'), '', 'password')}${field('expires_hours', t('expiresHours'), '0', 'number', { min: 0 })}${field('max_downloads', t('maxDownloads'), '0', 'number', { min: 0 })}${field('dir_password', t('directoryPassword'), state.pathPasswords[state.path] || '', 'password')}<div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="submit" class="btn btn--primary">${esc(t('createShare'))}</button></div></form>`);
  }
  function showShareCreated(id) {
    const url = `${location.origin}/s/${encodeURIComponent(id)}`;
    openModal(`${modalHead(t('shareCreated'))}<p class="picker-path">${esc(url)}</p><div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('close'))}</button><button type="button" class="btn btn--primary" data-act="copy-text" data-text="${esc(url)}">${esc(t('copyLink'))}</button></div>`);
  }
  async function openPreview(path, share = false, itemHint = null) {
    state.previewMode = 'view'; state.previewPath = path; state.previewShare = share; state.previewItem = itemHint;
    openModal(`${modalHead(t('preview'))}<div class="state-box"><div class="state-desc">${esc(t('loading'))}</div></div>`, { wide: true });
    try {
      let meta = itemHint || {};
      let resource = '';
      let blob;
      if (share) {
        const relative = path === '/' ? '' : path;
        resource = shareDownloadUrl(relative);
        const response = await fetchResource(resource, { share: true });
        blob = await response.blob();
        meta = { ...meta, name: meta.name || path.split('/').at(-1), type: meta.type || guessType(meta.name || path) };
      } else {
        const data = await api('/api/fs/get', { body: { path, password: state.pathPasswords[state.path] || '' }, headers: state.pathPasswords[state.path] ? { Password: state.pathPasswords[state.path] } : {} });
        meta = { ...data, ...itemHint, name: itemHint?.name || data.name, type: itemHint?.type || data.type, raw_url: data.raw_url };
        resource = safeResourceUrl(data.raw_url, path, data.sign);
        const response = await fetchResource(resource, { password: state.pathPasswords[state.path] || '' });
        blob = await response.blob();
      }
      const type = Number(meta.type) || guessType(meta.name || path);
      const ext = (meta.name || path).split('.').at(-1).toLowerCase();
      let body = '';
      if (type === 6 || blob.type.startsWith('image/')) {
        const url = URL.createObjectURL(blob); state.previewUrls.push(url);
        body = `<div class="preview-stage"><div class="preview-media"><img src="${esc(url)}" alt="${esc(meta.name || t('preview'))}" data-act="zoom-image"></div></div>`;
      } else if (type === 3 || blob.type.startsWith('video/')) {
        const url = URL.createObjectURL(blob); state.previewUrls.push(url);
        body = `<div class="preview-stage"><div class="preview-media"><video src="${esc(url)}" controls preload="metadata" aria-label="${esc(t('playback'))}"></video></div></div>`;
      } else if (type === 4 || blob.type.startsWith('audio/')) {
        const url = URL.createObjectURL(blob); state.previewUrls.push(url);
        body = `<div class="preview-stage"><div class="preview-media"><audio src="${esc(url)}" controls preload="metadata" aria-label="${esc(t('playback'))}"></audio></div></div>`;
      } else if (ext === 'pdf' || blob.type === 'application/pdf') {
        const url = URL.createObjectURL(blob); state.previewUrls.push(url);
        body = `<div class="preview-stage"><iframe class="preview-frame" src="${esc(url)}" title="${esc(meta.name || 'PDF')}" sandbox="allow-scripts"></iframe></div>`;
      } else if (type === 5 || /\.(md|markdown|txt|json|ya?ml|toml|ini|conf|log|csv|html?|css|js|mjs|ts|tsx|jsx|c|cc|cpp|h|hpp|sh|py|go|rs|xml|svg)$/i.test(meta.name || path)) {
        const text = await blob.text();
        const rendered = ext === 'md' || ext === 'markdown';
        body = `<div class="preview-stage">${rendered ? `<div class="md-prose preview-text" id="previewMarkdown">${renderMarkdown(text)}</div>` : `<pre class="code-view is-wrap" id="previewCode">${esc(text)}</pre>`}</div>`;
        if (state.canWrite && !share) body += `<div class="preview-toolbar"><button type="button" class="btn" data-act="edit-preview">${esc(t('edit'))}</button></div>`;
      } else {
        const url = URL.createObjectURL(blob); state.previewUrls.push(url);
        body = `<div class="preview-stage"><div class="state-box"><div class="state-desc">${esc(t('unsupportedPreview'))}</div><a class="btn btn--primary" href="${esc(url)}" download="${esc(meta.name || 'download')}">${esc(t('download'))}</a></div></div>`;
      }
      const info = `<div class="preview-toolbar"><div class="meta-pills"><span class="pill">${esc(formatBytes(meta.size ?? blob.size))}</span><span class="pill">${esc(formatDate(meta.modified))}</span></div><div class="preview-toolbar-actions"><button type="button" class="btn" data-act="download-current">${esc(t('download'))}</button>${!share ? `<button type="button" class="btn btn--ghost" data-act="file-properties" data-path="${esc(path)}">${esc(t('properties'))}</button>` : ''}</div></div>`;
      $('modalContent').innerHTML = `${modalHead(meta.name || path.split('/').at(-1) || t('preview'))}${info}${body}`;
    } catch (error) {
      $('modalContent').innerHTML = `${modalHead(t('preview'))}${stateBox(t('notFound'), error.message || t('unsupportedPreview'), `<button type="button" class="btn" data-act="close-modal">${esc(t('close'))}</button>`)}`;
    }
  }
  function guessType(name) {
    const ext = String(name).split('.').at(-1).toLowerCase();
    if (['jpg','jpeg','png','gif','webp','bmp','svg','avif','tif','tiff'].includes(ext)) return 6;
    if (['mp4','mkv','webm','mov','avi','m4v'].includes(ext)) return 3;
    if (['mp3','wav','ogg','flac','m4a','aac','opus'].includes(ext)) return 4;
    if (['zip','7z','rar','tar','gz'].includes(ext)) return 7;
    if (['pdf','doc','docx','xls','xlsx','ppt','pptx'].includes(ext)) return 2;
    if (['md','markdown','txt','json','yaml','yml','toml','ini','conf','log','csv','html','css','js','ts','tsx','jsx','c','cc','cpp','h','hpp','sh','py','go','rs','xml','svg'].includes(ext)) return 5;
    return 0;
  }
  function safeResourceUrl(raw, relativePath, sign = '') {
    let url = null;
    if (raw) {
      try { url = new URL(raw, location.href); } catch {}
    }
    if (url && url.origin === location.origin) return `${url.pathname}${url.search}`;
    const globalPath = pathJoin(state.user?.base_path || '/', relativePath);
    return `/d${encodePath(globalPath)}${sign ? `?sign=${encodeURIComponent(sign)}` : ''}`;
  }
  function shareDownloadUrl(relativePath) {
    const id = encodeURIComponent(state.share?.share_id || '');
    let url = `/sd/${id}`;
    const p = pathJoin('/', relativePath || '/');
    if (p !== '/') url += encodePath(p);
    if (state.shareToken) url += `${url.includes('?') ? '&' : '?'}token=${encodeURIComponent(state.shareToken)}`;
    return url;
  }
  async function downloadPath(path, share = false) {
    try {
      let url;
      if (share) url = shareDownloadUrl(path);
      else {
        const data = await api('/api/fs/get', { body: { path, password: state.pathPasswords[state.path] || '' } });
        url = safeResourceUrl(data.raw_url, path, data.sign);
      }
      const response = await fetchResource(url, { share, password: state.pathPasswords[state.path] || '' });
      const blob = await response.blob();
      const filename = path.split('/').filter(Boolean).at(-1) || 'download';
      const objectUrl = URL.createObjectURL(blob);
      const link = document.createElement('a'); link.href = objectUrl; link.download = filename; link.hidden = true;
      document.body.appendChild(link); link.click(); link.remove(); setTimeout(() => URL.revokeObjectURL(objectUrl), 30000);
    } catch (error) { toast(error.message || t('operationFailed'), 'err'); }
  }
  async function saveTextFile(path, text) {
    const encoded = encodeURIComponent(pathJoin('/', path));
    await new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest();
      xhr.open('PUT', '/api/fs/put'); xhr.setRequestHeader('File-Path', encoded); xhr.setRequestHeader('Content-Type', 'text/plain; charset=utf-8');
      xhr.setRequestHeader('Client-Id', clientId); if (state.token) xhr.setRequestHeader('Authorization', `Bearer ${state.token}`);
      const pw = state.pathPasswords[state.path]; if (pw) xhr.setRequestHeader('Password', pw);
      xhr.onload = () => {
        let payload; try { payload = JSON.parse(xhr.responseText); } catch {}
        if (xhr.status >= 200 && xhr.status < 300 && payload?.code === 200) resolve(); else reject(new ApiError(payload?.message || t('operationFailed'), payload?.code || xhr.status, xhr.status));
      };
      xhr.onerror = () => reject(new ApiError(t('operationFailed'))); xhr.send(text);
    });
  }
  async function editPreview() {
    const path = state.previewPath;
    try {
      const data = await api('/api/fs/get', { body: { path, password: state.pathPasswords[state.path] || '' } });
      const url = safeResourceUrl(data.raw_url, path, data.sign);
      const response = await fetchResource(url, { password: state.pathPasswords[state.path] || '' });
      const text = await response.text();
      state.previewMode = 'edit';
      $('modalContent').innerHTML = `${modalHead(path.split('/').at(-1))}<div class="preview-stage"><textarea class="code-editor" id="previewEditor" spellcheck="false">${esc(text)}</textarea></div><div class="modal-foot"><button type="button" class="btn" data-act="cancel-edit">${esc(t('cancelAction'))}</button><button type="button" class="btn btn--primary" data-act="save-preview">${esc(t('save'))}</button></div>`;
    } catch (error) { toast(error.message, 'err'); }
  }
  async function openArchive(path) {
    try {
      const data = await api('/api/fs/archive/list', { body: { path, password: state.pathPasswords[state.path] || '' } });
      state.archive = { path, entries: safeArray(data?.content) };
      const entries = state.archive.entries;
      const body = entries.length ? `<div class="archive-file-list">${entries.map((entry, index) => `<div class="archive-entry"><span class="file-icon">${fileIcon({ name: entry.path, is_dir: entry.is_dir, type: entry.is_dir ? 1 : guessType(entry.path) })}</span><span class="archive-entry-name">${esc(entry.path)}</span><span class="archive-entry-size">${entry.is_dir ? '—' : esc(formatBytes(entry.size))}</span><button type="button" class="btn btn--ghost btn--sm" data-act="archive-download" data-index="${index}">${esc(t('download'))}</button></div>`).join('')}</div>` : stateBox(t('archiveEmpty'), t('archiveContents'));
      openModal(`${modalHead(t('archive'))}<div class="panel-toolbar"><span class="field-help">${esc(path)}</span><button type="button" class="btn" data-act="archive-extract">${esc(t('decompress'))}</button></div>${body}`, { wide: true });
    } catch (error) { toast(error.message, 'err'); }
  }
  async function downloadArchiveEntry(index) {
    const entry = state.archive?.entries?.[index]; if (!entry) return;
    const url = `/ad${encodePath(state.archive.path)}?inner=${encodeURIComponent(entry.path)}&sign=${encodeURIComponent(entry.sign || '')}`;
    try {
      const response = await fetchResource(url);
      const blob = await response.blob(); const objectUrl = URL.createObjectURL(blob);
      const a = document.createElement('a'); a.href = objectUrl; a.download = entry.path.split('/').at(-1) || 'entry'; a.click();
      setTimeout(() => URL.revokeObjectURL(objectUrl), 30000);
    } catch (error) { toast(error.message, 'err'); }
  }

  function hideContext() { $('ctxMenu').hidden = true; $('ctxMenu').innerHTML = ''; }
  function showContext(path, x, y) {
    state.contextPath = path;
    const item = state.items.find(entry => itemFullPath(entry) === path);
    if (!item) return;
    const buttons = [
      `<button type="button" role="menuitem" class="ctx-item" data-act="ctx-open"><svg viewBox="0 0 20 20" aria-hidden="true"><path d="M3 10h13m-5-5 5 5-5 5"/></svg>${esc(item.is_dir ? t('open') : t('preview'))}</button>`,
      `<button type="button" role="menuitem" class="ctx-item" data-act="ctx-download"><svg viewBox="0 0 20 20" aria-hidden="true"><path d="M10 3v10m-4-4 4 4 4-4M4 16h12"/></svg>${esc(t('download'))}</button>`,
      `<button type="button" role="menuitem" class="ctx-item" data-act="ctx-share"><svg viewBox="0 0 20 20" aria-hidden="true"><circle cx="15" cy="4.5" r="2"/><circle cx="5" cy="10" r="2"/><circle cx="15" cy="15.5" r="2"/><path d="m7 9 6-3.5M7 11l6 3.5"/></svg>${esc(t('createShare'))}</button>`,
      `<div class="ctx-sep" role="separator"></div>`,
      `<button type="button" role="menuitem" class="ctx-item" data-act="ctx-rename"><svg viewBox="0 0 20 20" aria-hidden="true"><path d="m4 13.5-.8 3.3 3.3-.8L16 7.5 12.5 4zM11.5 5l3.5 3.5"/></svg>${esc(t('rename'))}</button>`,
      `<button type="button" role="menuitem" class="ctx-item is-danger" data-act="ctx-delete"><svg viewBox="0 0 20 20" aria-hidden="true"><path d="M4 6h12M8 6V4h4v2m-6 0 1 10h6l1-10"/></svg>${esc(t('delete'))}</button>`
    ];
    const menu = $('ctxMenu'); menu.innerHTML = buttons.join(''); menu.hidden = false;
    const rect = menu.getBoundingClientRect();
    menu.style.left = `${Math.max(8, Math.min(x, innerWidth - rect.width - 8))}px`;
    menu.style.top = `${Math.max(8, Math.min(y, innerHeight - rect.height - 8))}px`;
    menu.querySelector('[role="menuitem"]')?.focus();
  }

  async function toggleItemSelection(path) {
    if (state.selected.has(path)) state.selected.delete(path); else state.selected.add(path);
    renderExplorer();
  }
  async function removeNames(paths) {
    const byParent = new Map();
    for (const path of paths) {
      const parent = parentPath(path); if (!byParent.has(parent)) byParent.set(parent, []);
      byParent.get(parent).push(path.split('/').at(-1));
    }
    try {
      for (const [dir, names] of byParent) await api('/api/fs/remove', { body: { dir, names, password: state.pathPasswords[dir] || '' }, headers: state.pathPasswords[dir] ? { Password: state.pathPasswords[dir] } : {} });
      state.selected.clear(); closeModal(); toast(t('success')); await loadDir(state.path, true);
    } catch (error) { toast(error.message, 'err'); }
  }
  function openPicker(mode, names = [], srcDir = state.path) {
    state.picker = { mode, names, srcDir, path: '/' };
    openModal(`${modalHead(t('chooseFolder'))}<div id="pickerBody"></div><div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="button" class="btn btn--primary" data-act="picker-choose">${esc(t('useFolder'))}</button></div>`, { wide: true });
    loadPickerPath('/');
  }
  async function loadPickerPath(path) {
    if (!state.picker) return;
    state.picker.path = pathJoin('/', path);
    $('pickerBody').innerHTML = `<div class="picker-path">${esc(state.picker.path)}</div><div class="skeleton-list"><div class="skeleton-row"></div><div class="skeleton-row"></div></div>`;
    try {
      const items = await api('/api/fs/dirs', { body: { path: state.picker.path } });
      if (!state.picker || !$('pickerBody')) return;
      const dirs = safeArray(items).filter(item => item.name !== '..');
      $('pickerBody').innerHTML = `<div class="picker-path"><button type="button" class="btn btn--ghost btn--sm" data-act="picker-parent">↑ ${esc(t('parentFolder'))}</button><span>${esc(state.picker.path)}</span></div><div class="picker-list">${dirs.length ? dirs.map(item => `<button type="button" class="picker-item" data-act="picker-open" data-path="${esc(pathJoin(state.picker.path, item.name))}">${fileIcon({ ...item, is_dir: true, type: 1 })}<span>${esc(item.name)}</span></button>`).join('') : `<div class="state-box"><p class="state-desc">${esc(t('noFolders'))}</p></div>`}</div>`;
    } catch (error) { if (!state.picker || !$('pickerBody')) return; $('pickerBody').innerHTML = `<p class="field-help">${esc(error.message)}</p>`; }
  }
  async function pickerChoose() {
    if (!state.picker) return;
    const { mode, names, srcDir, path } = state.picker;
    try {
      if (mode === 'archive') {
        await api('/api/fs/archive/decompress', { body: { src_path: state.archive.path, dst_dir: path, password: state.pathPasswords[state.path] || '' } });
        state.archive = null; closeModal(); toast(t('success')); await loadDir(state.path, true); return;
      }
      await api(mode === 'move' ? '/api/fs/move' : '/api/fs/copy', { body: { src_dir: srcDir, dst_dir: path, names, as_task: mode === 'copy', password: state.pathPasswords[srcDir] || '' } });
      closeModal(); state.selected.clear(); toast(t('success')); await loadDir(state.path, true);
    } catch (error) { toast(error.message, 'err'); }
  }
  async function doBatchMoveCopy(mode) {
    const names = [...state.selected].map(path => path.split('/').at(-1));
    if (!names.length) return;
    openPicker(mode, names, state.path);
  }
  async function createShare(path, form) {
    const formData = new FormData(form);
    const body = { path, password: String(formData.get('password') || ''), expires_hours: Number(formData.get('expires_hours') || 0), max_downloads: Number(formData.get('max_downloads') || 0), dir_password: String(formData.get('dir_password') || '') };
    try {
      const data = await api('/api/share/create', { body }); closeModal(); showShareCreated(data.share_id); toast(t('shareCreated'));
    } catch (error) { toast(error.message, 'err'); }
  }
  async function submitForm(form, event) {
    event.preventDefault();
    const type = form.dataset.form;
    const fd = new FormData(form);
    if (type === 'login') {
      const body = { username: fd.get('username'), password: fd.get('password'), otp_code: fd.get('otp_code') };
      try {
        const data = await api('/api/auth/login', { body }); state.token = data.token; writeStore(STORAGE_KEY, state.token); closeModal();
        await refreshIdentity(); toast(t('success')); await loadDir(state.path, true);
      } catch (error) { toast(error.message, 'err'); $('modalContent').querySelector('[name="otp_code"]')?.focus(); }
    } else if (type === 'password-gate') {
      const path = form.dataset.path; const password = String(fd.get('password') || ''); state.pathPasswords[path] = password; closeModal(); await loadDir(path, false, true);
    } else if (type === 'mkdir') {
      const name = String(fd.get('name') || '').trim();
      if (!name || name.includes('/') || name === '.' || name === '..') { toast(t('operationFailed'), 'err'); return; }
      try { await api('/api/fs/mkdir', { body: { path: pathJoin(state.path, name) }, headers: state.pathPasswords[state.path] ? { Password: state.pathPasswords[state.path] } : {} }); closeModal(); toast(t('success')); await loadDir(state.path, true); }
      catch (error) { toast(error.message, 'err'); }
    } else if (type === 'new-file') {
      const name = String(fd.get('name') || '').trim();
      if (!name || name.includes('/') || name === '.' || name === '..') { toast(t('operationFailed'), 'err'); return; }
      try { await saveTextFile(pathJoin(state.path, name), String(fd.get('content') || '')); closeModal(); toast(t('success')); await loadDir(state.path, true); }
      catch (error) { toast(error.message, 'err'); }
    } else if (type === 'rename') {
      try { await api('/api/fs/rename', { body: { path: form.dataset.path, name: String(fd.get('name') || '').trim() }, headers: state.pathPasswords[state.path] ? { Password: state.pathPasswords[state.path] } : {} }); closeModal(); toast(t('success')); await loadDir(state.path, true); }
      catch (error) { toast(error.message, 'err'); }
    } else if (type === 'share-create') {
      await createShare(form.dataset.path, form);
    } else if (type === 'offline') {
      const urls = String(fd.get('urls') || '').split(/\r?\n/).map(item => item.trim()).filter(Boolean);
      if (!urls.length || urls.some(url => !/^https?:\/\//i.test(url))) { toast(t('invalidUrls'), 'err'); return; }
      try { await api('/api/fs/add_offline_download', { body: { path: state.path, urls, password: state.pathPasswords[state.path] || '' } }); closeModal(); toast(t('uploadQueued')); }
      catch (error) { toast(error.message, 'err'); }
    } else if (type === 'account-password') {
      const password = String(fd.get('password') || '');
      if (password !== String(fd.get('confirm') || '') || password.length < 6) { toast(t('operationFailed'), 'err'); return; }
      try { await api('/api/me/update', { body: { password } }); state.token = ''; state.user = null; removeStore(STORAGE_KEY); closeModal(); updateAuthButton(); openLogin(t('passwordChanged')); }
      catch (error) { toast(error.message, 'err'); }
    } else if (type === '2fa-verify') {
      try { await api('/api/auth/2fa/verify', { body: { secret: form.dataset.secret, code: fd.get('code') } }); closeModal(); await refreshIdentity(); toast(t('twoFactorEnabled')); }
      catch (error) { toast(error.message, 'err'); }
    } else if (type === '2fa-disable') {
      try { await api('/api/auth/2fa/cancel', { body: { code: fd.get('code') } }); closeModal(); await refreshIdentity(); toast(t('success')); }
      catch (error) { toast(error.message, 'err'); }
    } else if (type === 'storage') await saveStorage(form, fd);
    else if (type === 'user') await saveUser(form, fd);
    else if (type === 'role') await saveRole(form, fd);
    else if (type === 'meta') await saveMeta(form, fd);
    else if (type === 'settings') await saveSettings(form, fd);
  }

  async function refreshIdentity() {
    try { state.user = await api('/api/me'); }
    catch (error) { state.user = null; if (state.token && error.code === 401) { state.token = ''; removeStore(STORAGE_KEY); } }
    updateAuthButton(); renderSidebar();
  }
  async function loadInitial() {
    applyLanguage();
    setViewMode('explorer');
    if (state.announcementDismissed) $('announcementBar').hidden = true;
    try {
      state.settings = await api('/api/public/settings');
      $('siteTitle').textContent = state.settings.site_title || state.settings.app_name || APP;
      $('versionBadge').textContent = String(state.settings.version || 'v4.2.0').replace(/^v/, 'v').slice(0, 6);
      document.title = `${$('siteTitle').textContent} · Kova`;
      if (state.settings.announcement) { $('announcementText').textContent = state.settings.announcement; $('announcementBar').hidden = state.announcementDismissed; }
    } catch {
      state.backendUnavailable = true;
      $('announcementText').textContent = t('backendUnavailable');
      document.querySelector('[data-act="dismiss-announcement"]').hidden = true;
      $('announcementBar').hidden = false;
      $('btnAuth').disabled = true;
      $('btnTopAdmin').hidden = true;
    }
    await refreshIdentity();
    if (state.user && Number(state.user.role) === 2) $('btnTopAdmin').hidden = false;
    const match = location.pathname.match(/^\/s\/([^/]+)\/?$/);
    if (match) { await loadShare(decodeURIComponent(match[1])); return; }
    await loadDir('/');
  }

  async function loadShare(id) {
    setViewMode('share');
    $('shareHeading').textContent = t('sharedLink');
    $('shareMetaPills').innerHTML = ''; $('shareHeaderActions').innerHTML = '';
    $('shareBody').innerHTML = `<div class="skeleton-list"><div class="skeleton-row"></div><div class="skeleton-row"></div></div>`;
    try {
      state.share = await api(`/api/public/share/info?share_id=${encodeURIComponent(id)}`);
      state.shareSubpath = '/'; state.shareToken = ''; state.shareUnlocked = !state.share.has_password;
      document.title = `${state.share.name || t('sharedLink')} · Kova`;
      renderShare();
      if (state.share.is_dir && state.shareUnlocked) await loadShareDir('/');
    } catch (error) { $('shareBody').innerHTML = stateBox(t('notFound'), error.message); }
  }
  async function loadShareDir(path) {
    if (!state.share || !state.share.is_dir || !state.shareUnlocked) return;
    state.shareController?.abort(); state.shareController = new AbortController();
    const controller = state.shareController;
    $('shareCrumbBar').hidden = false; $('shareBody').innerHTML = `<div class="skeleton-list"><div class="skeleton-row"></div><div class="skeleton-row"></div></div>`;
    try {
      const data = await api('/api/public/share/list', { body: { share_id: state.share.share_id, path, token: state.shareToken }, headers: state.shareToken ? { 'X-Share-Token': state.shareToken } : {}, signal: controller.signal });
      if (controller.signal.aborted) return;
      state.shareSubpath = pathJoin('/', path); state.shareItems = safeArray(data?.content); renderShare();
    } catch (error) { if (error.name !== 'AbortError') $('shareBody').innerHTML = stateBox(t('notFound'), error.message); }
  }
  function renderShare() {
    if (!state.share) return;
    const share = state.share;
    $('shareHeading').textContent = share.name || t('sharedLink');
    $('shareMetaPills').innerHTML = `<span class="pill">${esc(share.is_dir ? t('folder') : t('file'))}</span>${!share.is_dir ? `<span class="pill">${esc(formatBytes(share.size))}</span>` : ''}<span class="pill">${esc(formatDate(share.modified))}</span><span class="pill">${esc(`${t('creator')}: ${share.creator || t('unknown')}`)}</span><span class="pill">${esc(`${t('views')}: ${share.views || 0}`)}</span>${share.has_password ? `<span class="pill pill--warn">${esc(t('protected'))}</span>` : ''}`;
    $('shareHeaderActions').innerHTML = !share.is_dir && state.shareUnlocked ? `<button type="button" class="btn btn--primary" data-act="share-download">${esc(t('download'))}</button>` : '';
    if (share.has_password && !state.shareUnlocked) {
      $('shareCrumbBar').hidden = true;
      $('shareBody').innerHTML = `<form data-form="share-auth" class="state-box"><div><h2 class="state-title">${esc(t('passwordGate'))}</h2><p class="state-desc">${esc(t('passwordGateDesc'))}</p></div>${field('password', t('password'), '', 'password', { required: true, autofocus: true })}<div class="state-actions"><button type="submit" class="btn btn--primary">${esc(t('unlock'))}</button></div></form>`;
      return;
    }
    if (!share.is_dir) {
      $('shareCrumbBar').hidden = true;
      $('shareBody').innerHTML = `<div class="state-box"><div class="state-icon">${fileIcon({ is_dir: false, type: share.type })}</div><h2 class="state-title">${esc(share.name)}</h2><p class="state-desc">${esc(`${formatBytes(share.size)} · ${formatDate(share.modified)}`)}</p><div class="state-actions"><button type="button" class="btn btn--primary" data-act="share-download">${esc(t('download'))}</button><button type="button" class="btn" data-act="share-preview">${esc(t('preview'))}</button></div></div>`;
      return;
    }
    $('shareCrumbBar').hidden = false;
    makeCrumbs($('shareCrumbs'), state.shareSubpath, true);
    $('shareViewTools').innerHTML = `<span class="folder-summary">${esc(t('itemCount', { n: state.shareItems.length }))}</span>`;
    if (!state.shareItems.length) { $('shareBody').innerHTML = stateBox(t('noItems'), t('noItemsDesc')); return; }
    $('shareBody').innerHTML = `<div class="table-wrap"><table class="file-table"><thead><tr><th>${esc(t('name'))}</th><th class="col-size">${esc(t('size'))}</th><th class="col-date">${esc(t('modified'))}</th><th class="col-ops">${esc(t('actions'))}</th></tr></thead><tbody>${state.shareItems.map((item, index) => {
      const path = pathJoin(state.shareSubpath, item.name);
      return `<tr class="file-row" data-item-path="${esc(path)}" data-index="${index}" tabindex="0" data-act="share-item-open"><td><div class="file-cell"><span class="file-icon">${fileIcon(item)}</span><span class="file-name-wrap"><span class="file-name">${esc(item.name)}</span><span class="file-sub">${esc(item.is_dir ? t('folder') : t('file'))}</span></span></div></td><td class="col-size">${item.is_dir ? '—' : esc(formatBytes(item.size))}</td><td class="col-date">${esc(formatDate(item.modified))}</td><td class="col-ops"><button type="button" class="btn btn--ghost btn--sm" data-act="share-item-preview" data-path="${esc(path)}" data-index="${index}">${esc(item.is_dir ? t('open') : t('preview'))}</button></td></tr>`;
    }).join('')}</tbody></table></div>`;
  }
  async function submitShareAuth(form, fd) {
    try {
      const data = await api('/api/public/share/auth', { body: { share_id: state.share.share_id, password: fd.get('password') } });
      state.shareToken = data.token; state.shareUnlocked = true; renderShare();
      if (state.share.is_dir) await loadShareDir('/');
    } catch (error) { toast(error.message, 'err'); }
  }
  async function previewShareItem(path, item) {
    if (item?.is_dir) { state.shareSubpath = path; await loadShareDir(path); return; }
    await openPreview(path, true, item);
  }
  async function doDownloadShare() {
    const path = state.share.is_dir ? (state.shareSubpath === '/' ? '/' : state.shareSubpath) : '/';
    await downloadPath(path, true);
  }

  function openAccount() {
    if (!state.user) { openLogin(); return; }
    const isGuest = Number(state.user.role) === 1;
    openModal(`${modalHead(t('accountTitle'), `${t('currentUser')}: ${state.user.username}`)}<div class="account-actions">
      ${!isGuest ? (!state.user.otp ? `<button type="button" class="btn" data-act="setup-2fa">${esc(t('enable2fa'))}</button>` : `<button type="button" class="btn" data-act="disable-2fa">${esc(t('disable2fa'))}</button>`) : ''}
      ${!isGuest ? `<button type="button" class="btn" data-act="change-password">${esc(t('changePassword'))}</button><button type="button" class="btn btn--ghost" data-act="account-sessions">${esc(t('sessions'))}</button><button type="button" class="btn btn--ghost" data-act="account-shares">${esc(t('userShares'))}</button><button type="button" class="btn btn--ghost" data-act="account-tasks">${esc(t('userTasks'))}</button>` : ''}
      <button type="button" class="btn btn--danger" data-act="sign-out">${esc(t('logOut'))}</button></div>`, { wide: true });
  }
  async function setup2fa() {
    try {
      const data = await api('/api/auth/2fa/generate', { body: {} });
      openModal(`<form data-form="2fa-verify" data-secret="${esc(data.secret)}">${modalHead(t('twoFactor'), t('authenticatorHint'))}<p class="picker-path">${esc(data.secret)}</p><p class="field-help">${esc(data.qr)}</p>${field('code', t('code'), '', 'text', { required: true, autocomplete: 'one-time-code' })}<div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="submit" class="btn btn--primary">${esc(t('verify2fa'))}</button></div></form>`);
    } catch (error) { toast(error.message, 'err'); }
  }
  function showDisable2fa() {
    openModal(`<form data-form="2fa-disable">${modalHead(t('disable2fa'))}${field('code', t('code'), '', 'text', { required: true, autocomplete: 'one-time-code' })}<div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="submit" class="btn btn--danger">${esc(t('disable2fa'))}</button></div></form>`);
  }
  function showChangePassword() {
    openModal(`<form data-form="account-password">${modalHead(t('changePassword'))}${field('password', t('passwordNew'), '', 'password', { required: true, min: 6, autocomplete: 'new-password' })}${field('confirm', t('passwordConfirm'), '', 'password', { required: true, min: 6, autocomplete: 'new-password' })}<div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="submit" class="btn btn--primary">${esc(t('save'))}</button></div></form>`);
  }
  async function showSessions() {
    try {
      const rows = await api('/api/me/sessions');
      openModal(`${modalHead(t('sessions'))}<div class="session-list">${safeArray(rows).map(row => `<div class="session-row"><div><strong>${esc(row.user_agent || t('unknown'))}</strong><div class="field-help">${esc(row.ip || '')} · ${esc(formatDate(row.last_active))} ${row.current ? `· ${esc(t('currentSession'))}` : ''}</div></div>${row.current ? '' : `<button type="button" class="btn btn--danger btn--sm" data-act="revoke-session" data-key="${esc(row.device_key)}">${esc(t('revoke'))}</button>`}</div>`).join('') || `<p class="field-help">${esc(t('noSessions'))}</p>`}</div>`, { wide: true });
    } catch (error) { toast(error.message, 'err'); }
  }
  async function showUserShares() {
    try {
      const data = await api('/api/share/list'); const rows = safeArray(data?.content);
      openModal(`${modalHead(t('userShares'))}<div class="admin-table-wrap">${rows.length ? `<table class="file-table"><thead><tr><th>${esc(t('name'))}</th><th>${esc(t('views'))}</th><th>${esc(t('downloads'))}</th><th></th></tr></thead><tbody>${rows.map(row => `<tr><td>${esc(row.path)}</td><td>${esc(row.views)}</td><td>${esc(row.downloads)}</td><td><button type="button" class="btn btn--ghost btn--sm" data-act="copy-share-link" data-id="${esc(row.share_id)}">${esc(t('copyLink'))}</button></td></tr>`).join('')}</tbody></table>` : `<p class="field-help">${esc(t('noShares'))}</p>`}</div>`, { wide: true });
    } catch (error) { toast(error.message, 'err'); }
  }
  async function showUserTasks() {
    try {
      const [active, done] = await Promise.all([api('/api/task/undone'), api('/api/task/done')]);
      const rows = [...safeArray(active), ...safeArray(done)];
      openModal(`${modalHead(t('userTasks'))}<div class="admin-table-wrap">${rows.length ? `<table class="file-table"><thead><tr><th>${esc(t('name'))}</th><th>${esc(t('taskType'))}</th><th>${esc(t('status'))}</th><th>${esc(t('progress'))}</th></tr></thead><tbody>${rows.map(row => `<tr><td>${esc(row.name)}</td><td>${esc(row.type)}</td><td>${esc(row.status || row.state)}</td><td>${Math.round(Number(row.progress || 0) * 100)}%</td></tr>`).join('')}</tbody></table>` : `<p class="field-help">${esc(t('noTasks'))}</p>`}</div>`, { wide: true });
    } catch (error) { toast(error.message, 'err'); }
  }

  async function getDroppedFiles(dataTransfer) {
    const entries = [];
    const walkEntry = entry => new Promise(resolve => {
      if (!entry) { resolve(); return; }
      if (entry.isFile) {
        entry.file(file => { entries.push({ file, relativePath: entry.fullPath.replace(/^\/+/, '') }); resolve(); }, () => resolve());
      } else if (entry.isDirectory) {
        const reader = entry.createReader(); const readBatch = () => reader.readEntries(async children => {
          if (!children.length) { resolve(); return; }
          for (const child of children) await walkEntry(child);
          readBatch();
        }, () => resolve());
        readBatch();
      } else resolve();
    });
    const items = Array.from(dataTransfer?.items || []);
    if (items.length) {
      for (const item of items) {
        const entry = item.webkitGetAsEntry?.();
        if (entry) await walkEntry(entry);
        else { const file = item.getAsFile?.(); if (file) entries.push({ file, relativePath: file.name }); }
      }
    } else {
      for (const file of Array.from(dataTransfer?.files || [])) entries.push({ file, relativePath: file.webkitRelativePath || file.name });
    }
    return entries;
  }
  function enqueueFiles(files) {
    const rows = files.filter(row => row?.file instanceof File).map(row => ({
      id: globalThis.crypto?.randomUUID?.() || `${Date.now()}-${Math.random()}`,
      file: row.file,
      path: pathJoin(state.path, row.relativePath || row.file.name),
      name: row.relativePath || row.file.name,
      status: 'queued', loaded: 0, speed: 0, error: ''
    }));
    if (!rows.length) return;
    state.uploadQueue.push(...rows);
    state.uploadCancel = false; state.uploadStarted = Date.now(); state.uploadBytes = 0;
    $('uploadDock').hidden = false; $('btnUploadCancel').hidden = false; $('btnUploadClose').hidden = true;
    renderUploadQueue(); processUploadQueue();
  }
  function renderUploadQueue() {
    const total = state.uploadQueue.length;
    const done = state.uploadQueue.filter(row => ['done', 'failed', 'canceled'].includes(row.status)).length;
    const active = state.uploadQueue.find(row => row.status === 'uploading');
    const current = active || state.uploadQueue.find(row => row.status === 'queued');
    $('uploadSummaryTitle').textContent = active ? t('uploadingFile', { name: active.name }) : done >= total ? t('uploadDone') : t('uploading');
    $('uploadMeta').textContent = total ? t('uploadCount', { done, total }) + (active ? ` · ${t('speed', { speed: formatBytes(active.speed) })}` : '') : '';
    const progress = active && active.file.size ? Math.max(0, Math.min(1, active.loaded / active.file.size)) : (done === total && total ? 1 : 0);
    $('uploadProgressBar').style.transform = `scaleX(${progress})`;
    $('uploadDot').classList.toggle('is-live', Boolean(active));
    $('uploadDot').classList.toggle('is-warn', state.uploadQueue.some(row => row.status === 'failed'));
    $('uploadQueueList').innerHTML = state.uploadQueue.slice(-12).reverse().map(row => `<div class="upload-queue-row"><span title="${esc(row.name)}">${esc(row.name)}</span><span>${esc(row.status === 'done' ? '✓' : row.status === 'failed' ? '!' : row.status === 'canceled' ? '–' : row.status === 'uploading' ? `${Math.round(row.loaded / Math.max(1, row.file.size) * 100)}%` : '…')}</span></div>`).join('');
  }
  async function ensureUploadParents(path) {
    const parent = parentPath(path); const base = pathJoin('/', state.path);
    if (parent === base || parent === '/') return;
    const relative = parent.startsWith(base === '/' ? '/' : `${base}/`) ? parent.slice(base === '/' ? 1 : base.length + 1) : parent.slice(1);
    let current = base;
    for (const segment of relative.split('/').filter(Boolean)) {
      current = pathJoin(current, segment);
      try { await api('/api/fs/mkdir', { body: { path: current }, headers: state.pathPasswords[base] ? { Password: state.pathPasswords[base] } : {} }); }
      catch (error) {
        if (!/exist|already|duplicate/i.test(error.message)) throw error;
      }
    }
  }
  function uploadFile(row) {
    return new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest(); state.uploadXhr = xhr;
      xhr.open('PUT', '/api/fs/put');
      xhr.setRequestHeader('File-Path', encodeURIComponent(pathJoin('/', row.path)));
      xhr.setRequestHeader('Content-Type', row.file.type || 'application/octet-stream');
      xhr.setRequestHeader('Client-Id', clientId);
      if (state.token) xhr.setRequestHeader('Authorization', `Bearer ${state.token}`);
      const password = state.pathPasswords[state.path]; if (password) xhr.setRequestHeader('Password', password);
      const started = performance.now();
      xhr.upload.onprogress = event => {
        if (event.lengthComputable) row.loaded = event.loaded;
        const elapsed = Math.max(0.001, (performance.now() - started) / 1000);
        row.speed = row.loaded / elapsed;
        renderUploadQueue();
      };
      xhr.onload = () => {
        state.uploadXhr = null;
        let payload; try { payload = JSON.parse(xhr.responseText); } catch {}
        if (xhr.status >= 200 && xhr.status < 300 && payload?.code === 200) { row.loaded = row.file.size; row.status = 'done'; resolve(); }
        else { row.status = 'failed'; row.error = payload?.message || `${xhr.status}`; reject(new ApiError(row.error, payload?.code || xhr.status, xhr.status)); }
      };
      xhr.onerror = () => { state.uploadXhr = null; row.status = 'failed'; row.error = t('operationFailed'); reject(new ApiError(t('operationFailed'))); };
      xhr.onabort = () => { state.uploadXhr = null; row.status = 'canceled'; resolve(); };
      xhr.send(row.file);
    });
  }
  async function processUploadQueue() {
    if (state.uploadRunning) return;
    state.uploadRunning = true;
    try {
      for (const row of state.uploadQueue) {
        if (state.uploadCancel) break;
        if (row.status !== 'queued') continue;
        row.status = 'uploading'; renderUploadQueue();
        try { await ensureUploadParents(row.path); await uploadFile(row); }
        catch (error) { if (row.status !== 'canceled') { row.status = 'failed'; row.error = error.message; toast(`${t('uploadFailed')}: ${row.name} · ${error.message}`, 'err', 6000); } }
        renderUploadQueue();
      }
    } finally {
      state.uploadRunning = false;
      if (state.uploadCancel) for (const row of state.uploadQueue) if (row.status === 'queued') row.status = 'canceled';
      const finished = state.uploadQueue.every(row => ['done', 'failed', 'canceled'].includes(row.status));
      if (finished) {
        $('uploadSummaryTitle').textContent = state.uploadQueue.some(row => row.status === 'failed') ? t('uploadFailed') : t('uploadDone');
        $('btnUploadCancel').hidden = true; $('btnUploadClose').hidden = false;
        $('uploadDot').classList.remove('is-live');
        if (state.uploadQueue.some(row => row.status === 'done')) { toast(t('uploadDone')); if (state.currentView === 'explorer') await loadDir(state.path, true); }
      }
      renderUploadQueue();
    }
  }
  function cancelUploads() {
    state.uploadCancel = true;
    for (const row of state.uploadQueue) if (row.status === 'queued') row.status = 'canceled';
    state.uploadXhr?.abort(); renderUploadQueue();
  }

  function openAdmin(tab = 'overview') {
    if (!state.user || Number(state.user.role) !== 2) { toast(t('noPermission'), 'err'); return; }
    setViewMode('admin'); selectAdminTab(tab); $('mainStage').focus({ preventScroll: true });
  }
  function stopAdminTimer() { clearInterval(state.adminTimer); state.adminTimer = 0; }
  function selectAdminTab(tab) {
    const allowed = ['overview', 'storages', 'users', 'roles', 'metas', 'shares', 'tasks', 'settings'];
    state.adminTab = allowed.includes(tab) ? tab : 'overview';
    for (const button of $('adminTabs').querySelectorAll('[role="tab"]')) {
      const active = button.dataset.tab === state.adminTab;
      button.classList.toggle('is-active', active); button.setAttribute('aria-selected', String(active));
    }
    const activeTab = $('adminTabs').querySelector(`[data-tab="${state.adminTab}"]`);
    const indicator = $('adminTabIndicator');
    if (activeTab && indicator) { indicator.style.width = `${activeTab.offsetWidth}px`; indicator.style.transform = `translateX(${activeTab.offsetLeft - 4}px)`; }
    loadAdminPanel();
  }
  async function loadAdminPanel() {
    if (state.currentView !== 'admin') return;
    stopAdminTimer();
    const panel = $('adminPanelBody');
    panel.innerHTML = `<div class="skeleton-list"><div class="skeleton-row"></div><div class="skeleton-row"></div></div>`;
    try {
      if (state.adminTab === 'overview') await renderAdminOverview();
      else if (state.adminTab === 'storages') await renderAdminStorages();
      else if (state.adminTab === 'users') await renderAdminUsers();
      else if (state.adminTab === 'roles') await renderAdminRoles();
      else if (state.adminTab === 'metas') await renderAdminMetas();
      else if (state.adminTab === 'shares') await renderAdminShares();
      else if (state.adminTab === 'tasks') await renderAdminTasks();
      else await renderAdminSettings();
    } catch (error) { panel.innerHTML = stateBox(t('error'), error.message, `<button type="button" class="btn" data-act="refresh-admin">${esc(t('retry'))}</button>`); }
  }
  function panelToolbar(title, actions = '') { return `<div class="panel-toolbar"><h2 class="panel-heading">${esc(title)}</h2><div class="stage-actions">${actions}</div></div>`; }
  function statCard(label, value, foot = '') { return `<article class="stat-card"><div class="stat-label">${esc(label)}</div><div class="stat-value">${esc(value)}</div>${foot ? `<div class="stat-foot">${esc(foot)}</div>` : ''}</article>`; }
  function telemetryValue(stats, key, format = value => String(value ?? '—')) { return format(stats?.[key]); }
  async function renderAdminOverview() {
    const stats = await api('/api/admin/system/stats'); state.adminData.stats = stats;
    const panel = $('adminPanelBody');
    const actions = `<button type="button" class="btn" data-act="telemetry-toggle">${esc(t(state.telemetryPaused ? 'resume' : 'pause'))}</button><button type="button" class="btn" data-act="refresh-admin">${esc(t('refreshNow'))}</button>`;
    panel.innerHTML = `${panelToolbar(t('telemetry'), actions)}<div class="stats-grid">
      ${statCard(t('uptime'), formatDuration(stats.uptime_sec), `${t('threads')}: ${stats.threads} · ${t('cpuCores')}: ${stats.cpu_cores}`)}
      ${statCard(t('memory'), formatBytes(stats.rss_bytes), `${t('peakMemory')}: ${formatBytes(stats.peak_rss_bytes)}`)}
      ${statCard(t('activeConnections'), stats.active_connections, `${t('totalRequests')}: ${Number(stats.total_requests || 0).toLocaleString()}`)}
      ${statCard(t('sent'), formatBytes(stats.bytes_sent), `${t('received')}: ${formatBytes(stats.bytes_received)}`)}
      ${statCard(t('mounted'), stats.mounted_storages, `${t('database')}: ${formatBytes(stats.db_size_bytes)}`)}
      ${statCard(t('cache'), stats.vfs_cache_entries, `${t('cacheHits')}: ${stats.vfs_cache_hits} · ${t('cacheMisses')}: ${stats.vfs_cache_misses}`)}
      ${statCard(t('indexed'), Number(stats.fts_indexed_files || 0).toLocaleString(), `${t('activeTasks')}: ${stats.tasks_undone} · ${t('completedTasks')}: ${stats.tasks_done}`)}
      ${statCard(APP, stats.version, stats.engine || '')}
      </div><p class="field-help" aria-live="polite">${esc(t('telemetryUnavailable'))}</p>`;
    if (!state.telemetryPaused) state.adminTimer = setInterval(async () => {
      if (state.currentView !== 'admin' || state.adminTab !== 'overview' || state.telemetryPaused) return;
      try {
        const next = await api('/api/admin/system/stats');
        if (state.currentView === 'admin' && state.adminTab === 'overview') { state.adminData.stats = next; renderAdminOverviewFromCache(); }
      } catch {}
    }, 4000);
  }
  function renderAdminOverviewFromCache() {
    const s = state.adminData.stats; if (!s) return;
    const cards = $('adminPanelBody').querySelector('.stats-grid'); if (!cards) return;
    cards.innerHTML = `${statCard(t('uptime'), formatDuration(s.uptime_sec), `${t('threads')}: ${s.threads} · ${t('cpuCores')}: ${s.cpu_cores}`)}${statCard(t('memory'), formatBytes(s.rss_bytes), `${t('peakMemory')}: ${formatBytes(s.peak_rss_bytes)}`)}${statCard(t('activeConnections'), s.active_connections, `${t('totalRequests')}: ${Number(s.total_requests || 0).toLocaleString()}`)}${statCard(t('sent'), formatBytes(s.bytes_sent), `${t('received')}: ${formatBytes(s.bytes_received)}`)}${statCard(t('mounted'), s.mounted_storages, `${t('database')}: ${formatBytes(s.db_size_bytes)}`)}${statCard(t('cache'), s.vfs_cache_entries, `${t('cacheHits')}: ${s.vfs_cache_hits} · ${t('cacheMisses')}: ${s.vfs_cache_misses}`)}${statCard(t('indexed'), Number(s.fts_indexed_files || 0).toLocaleString(), `${t('activeTasks')}: ${s.tasks_undone} · ${t('completedTasks')}: ${s.tasks_done}`)}${statCard(APP, s.version, s.engine || '')}`;
  }
  function adminTable(headers, rows) {
    return `<div class="admin-table-wrap"><table class="file-table"><thead><tr>${headers.map(h => `<th>${esc(h)}</th>`).join('')}</tr></thead><tbody>${rows}</tbody></table></div>`;
  }
  function actionButton(action, label, id, danger = false, extra = '') {
    return `<button type="button" class="btn ${danger ? 'btn--danger' : 'btn--ghost'} btn--sm" data-act="${esc(action)}" data-id="${esc(id)}" ${extra}>${esc(label)}</button>`;
  }
  async function renderAdminStorages() {
    const [data, drivers] = await Promise.all([api('/api/admin/storage/list'), api('/api/admin/driver/list')]);
    state.adminData.drivers = safeArray(drivers); const rows = safeArray(data?.content);
    const buttons = `<button type="button" class="btn" data-act="storage-new">${esc(t('addStorage'))}</button><button type="button" class="btn btn--ghost" data-act="storage-reload">${esc(t('reloadStorages'))}</button>`;
    const body = rows.map(row => `<tr><td>${driverIcon(row.driver)}<strong>${esc(row.mount_path)}</strong></td><td>${esc(driverTitle(row.driver))}</td><td><span class="pill ${row.status === 'work' ? 'pill--ok' : 'pill--warn'}">${esc(row.status || t('unknown'))}</span></td><td>${esc(row.remark || '')}</td><td><div class="row-actions">${actionButton('storage-edit', t('edit'), row.id)}${actionButton('storage-delete', t('delete'), row.id, true)}</div></td></tr>`).join('');
    $('adminPanelBody').innerHTML = `${panelToolbar(t('storages'), buttons)}${rows.length ? adminTable([t('mountPath'), t('driver'), t('status'), t('remark'), t('actions')], body) : stateBox(t('noStorages'), t('noItemsDesc'))}`;
  }
  async function storageDialog(id = 0) {
    let data = null;
    try { if (id) data = await api(`/api/admin/storage/get?id=${encodeURIComponent(id)}`); } catch (error) { toast(error.message, 'err'); return; }
    const drivers = state.adminData.drivers || await api('/api/admin/driver/list'); state.adminData.drivers = drivers;
    state.storageForm = { id, data, addition: parseJson(data?.addition, {}) };
    const options = drivers.map(driver => `<option value="${esc(driver.name)}" ${driver.name === (data?.driver || 'Local') ? 'selected' : ''}>${esc(driver.display_name || driver.name)}</option>`).join('');
    openModal(`<form data-form="storage" data-id="${esc(id)}">${modalHead(id ? t('updateStorage') : t('createStorage'))}<div class="form-grid">${field('mount_path', t('mountPath'), data?.mount_path || '/', 'text', { required: true })}${field('order', t('order'), data?.order ?? 0, 'number')}
      <label class="field"><span class="field-label">${esc(t('driver'))}</span><select class="input" id="storageDriver" name="driver">${options}</select></label>${field('cache_expiration', t('cacheExpiration'), data?.cache_expiration ?? 30, 'number', { min: 0 })}
      ${field('remark', t('remark'), data?.remark || '', 'text')}${field('order_by', t('orderBy'), data?.order_by || 'name', 'text')}
      <label class="field"><span class="field-label">${esc(t('direction'))}</span><select class="input" name="order_direction"><option ${data?.order_direction !== 'DESC' ? 'selected' : ''}>ASC</option><option ${data?.order_direction === 'DESC' ? 'selected' : ''}>DESC</option></select></label></div>
      <div class="form-grid">${checkbox('enable_sign', t('signUrls'), Boolean(data?.enable_sign))}${checkbox('web_proxy', t('webProxy'), Boolean(data?.web_proxy))}${id ? checkbox('disabled', t('disabled'), Boolean(data?.disabled)) : ''}</div>
      <hr class="divider"><h3 class="panel-heading">${esc(t('config'))}</h3><div id="driverFields"></div><div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="submit" class="btn btn--primary">${esc(t('save'))}</button></div></form>`, { wide: true });
    renderDriverFields(data?.driver || 'Local');
  }
  function parseJson(text, fallback = {}) { try { const data = typeof text === 'string' ? JSON.parse(text) : text; return data && typeof data === 'object' ? data : fallback; } catch { return fallback; } }
  function renderDriverFields(name) {
    const driver = (state.adminData.drivers || []).find(entry => entry.name === name);
    const values = state.storageForm?.addition || {};
    $('driverFields').innerHTML = driver?.fields?.length ? `<div class="form-grid">${driver.fields.map(item => {
      const value = values[item.name] ?? item.default_val ?? '';
      if (item.type === 'bool') return `<div class="field"><span class="field-label">${esc(item.label)}</span>${checkbox(item.name, item.help_text || item.label, value === true || value === 'true')}<span class="field-help">${esc(item.help_text || '')}</span></div>`;
      if (item.type === 'select') return `<label class="field"><span class="field-label">${esc(item.label)}</span><select class="input" name="driver_field_${esc(item.name)}">${String(item.options || '').split(',').map(option => option.trim()).filter(Boolean).map(option => `<option value="${esc(option)}" ${String(value) === option ? 'selected' : ''}>${esc(option)}</option>`).join('')}</select><span class="field-help">${esc(item.help_text || '')}</span></label>`;
      const type = item.type === 'number' ? 'number' : item.type === 'password' ? 'password' : 'text';
      const input = item.type === 'text' ? 'textarea' : type;
      const id = `driver_${item.name}`;
      return `${field(`driver_field_${item.name}`, item.label, value, input, { id, required: Boolean(item.required) })}<span class="field-help">${esc(item.help_text || '')}</span>`;
    }).join('')}</div>` : `<p class="field-help">${esc(t('unknown'))}</p>`;
  }
  async function saveStorage(form, fd) {
    const driver = String(fd.get('driver') || 'Local'); const schema = (state.adminData.drivers || []).find(entry => entry.name === driver)?.fields || [];
    const addition = {};
    for (const item of schema) {
      if (item.type === 'bool') addition[item.name] = fd.get(item.name) === 'on';
      else addition[item.name] = fd.get(`driver_field_${item.name}`) ?? item.default_val ?? '';
    }
    const id = Number(form.dataset.id || 0);
    const body = { mount_path: fd.get('mount_path'), order: Number(fd.get('order') || 0), driver, cache_expiration: Number(fd.get('cache_expiration') || 0), remark: fd.get('remark') || '', order_by: fd.get('order_by') || 'name', order_direction: fd.get('order_direction') || 'ASC', enable_sign: fd.get('enable_sign') === 'on', web_proxy: fd.get('web_proxy') === 'on', addition };
    try {
      const response = await api(`/api/admin/storage/${id ? 'update' : 'create'}`, { body: id ? { ...body, id, disabled: fd.get('disabled') === 'on' } : body });
      closeModal(); await renderAdminStorages();
      if (response?.warning) toast(t('storageWarning', { message: response.warning }), 'err', 6000); else toast(t('storageSaved'));
    } catch (error) { toast(error.message, 'err'); }
  }
  async function renderAdminUsers() {
    const data = await api('/api/admin/user/list'); const rows = safeArray(data?.content);
    const buttons = `<button type="button" class="btn btn--ghost" data-act="guest-toggle">${esc(t('guestAccess'))}</button><button type="button" class="btn" data-act="user-new">${esc(t('addUser'))}</button>`;
    const rowHtml = rows.map(row => `<tr><td><strong>${esc(row.username)}</strong>${row.otp ? ` <span class="pill pill--ok">2FA</span>` : ''}</td><td>${esc(row.base_path)}</td><td>${esc(row.role_ids?.join(', ') || '—')}</td><td>${row.disabled ? `<span class="pill pill--warn">${esc(t('disabled'))}</span>` : `<span class="pill pill--ok">${esc(t('enabled'))}</span>`}</td><td><div class="row-actions">${actionButton('user-edit', t('edit'), row.id)}${row.otp ? actionButton('user-reset-2fa', t('reset2fa'), row.id) : ''}${Number(row.role) === 0 ? actionButton('user-delete', t('delete'), row.id, true) : ''}</div></td></tr>`).join('');
    $('adminPanelBody').innerHTML = `${panelToolbar(t('users'), buttons)}${rows.length ? adminTable([t('username'), t('basePath'), t('rolesAssigned'), t('status'), t('actions')], rowHtml) : stateBox(t('noUsers'), '')}`;
  }
  async function userDialog(id = 0) {
    const [usersData, rolesData] = await Promise.all([id ? api('/api/admin/user/list') : Promise.resolve(null), api('/api/admin/role/list')]);
    const user = id ? safeArray(usersData?.content).find(item => Number(item.id) === Number(id)) : null;
    if (id && !user) { toast(t('notFound'), 'err'); return; }
    const roles = safeArray(rolesData?.content);
    const roleBoxes = roles.map(role => checkbox('role_id', role.name, safeArray(user?.role_ids).map(Number).includes(Number(role.id)), `value="${esc(role.id)}"`)).join('');
    openModal(`<form data-form="user" data-id="${esc(id)}">${modalHead(id ? t('updateUser') : t('createUser'))}<div class="form-grid">${field('username', t('username'), user?.username || '', 'text', { required: !id, autocomplete: 'username' })}${id ? '' : field('password', t('password'), '', 'password', { required: true, min: 6 })}${field('base_path', t('basePath'), user?.base_path || '/', 'text')}</div><div class="perm-grid">${roleBoxes || `<span class="field-help">${esc(t('noRoles'))}</span>`}</div>${id ? checkbox('disabled', t('disabled'), Boolean(user?.disabled), `${Number(user?.role) === 2 ? 'disabled' : ''}`) : ''}${id ? field('password', t('passwordNew'), '', 'password', { min: 6 }) : ''}<div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="submit" class="btn btn--primary">${esc(t('save'))}</button></div></form>`, { wide: true });
  }
  async function saveUser(form, fd) {
    const id = Number(form.dataset.id || 0); const role_ids = fd.getAll('role_id').map(Number);
    const body = { username: fd.get('username'), base_path: fd.get('base_path') || '/', role_ids, disabled: fd.get('disabled') === 'on' };
    if (fd.get('password')) body.password = fd.get('password');
    try { await api(`/api/admin/user/${id ? 'update' : 'create'}`, { body: id ? { ...body, id } : body }); closeModal(); toast(t('success')); await renderAdminUsers(); }
    catch (error) { toast(error.message, 'err'); }
  }
  async function toggleGuest() {
    try {
      const data = await api('/api/admin/user/list'); const guest = safeArray(data?.content).find(user => Number(user.role) === 1);
      if (!guest) { toast(t('notFound'), 'err'); return; }
      await api('/api/admin/user/update', { body: { id: guest.id, base_path: guest.base_path, disabled: !guest.disabled } });
      toast(guest.disabled ? t('guestEnabled') : t('guestDisabled')); await renderAdminUsers();
    } catch (error) { toast(error.message, 'err'); }
  }
  async function renderAdminRoles() {
    const data = await api('/api/admin/role/list'); const rows = safeArray(data?.content);
    const body = rows.map(row => `<tr><td><strong>${esc(row.name)}</strong></td><td>${esc(row.description || '')}</td><td>${esc(safeArray(row.permission_scopes).length)}</td><td><div class="row-actions">${actionButton('role-edit', t('edit'), row.id)}${actionButton('role-delete', t('delete'), row.id, true)}</div></td></tr>`).join('');
    $('adminPanelBody').innerHTML = `${panelToolbar(t('roles'), `<button type="button" class="btn" data-act="role-new">${esc(t('addRole'))}</button>`)}${rows.length ? adminTable([t('roleName'), t('description'), t('permissionScopes'), t('actions')], body) : stateBox(t('noRoles'), '')}`;
  }
  const PERMISSIONS = [
    'seeHidden', 'accessWithoutPassword', 'offlineDownloads', 'write', 'renamePermission', 'move', 'copy', 'removePermission',
    'webdavRead', 'webdavManage', 'ftpAccess', 'ftpManage', 'readArchives', 'decompressPermission', 'pathLimit', 'mcpAccess', 'mcpManage'
  ];
  function roleDialog(role = null) {
    const scope = safeArray(role?.permission_scopes)[0] || { path: '/', permission: 0 };
    const grid = PERMISSIONS.map((key, bit) => checkbox(`perm_${bit}`, t(key), (Number(scope.permission) & (1 << bit)) !== 0)).join('');
    openModal(`<form data-form="role" data-id="${esc(role?.id || 0)}">${modalHead(role ? t('edit') : t('addRole'))}<div class="form-grid">${field('name', t('roleName'), role?.name || '', 'text', { required: true })}${field('description', t('description'), role?.description || '', 'text')}${field('path', t('scopePath'), scope.path || '/', 'text', { required: true })}</div><div class="perm-grid">${grid}</div><div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="submit" class="btn btn--primary">${esc(t('save'))}</button></div></form>`, { wide: true });
  }
  async function saveRole(form, fd) {
    let permission = 0; for (let bit = 0; bit < 17; bit++) if (fd.get(`perm_${bit}`) === 'on') permission |= (1 << bit);
    const id = Number(form.dataset.id || 0); const body = { id, name: fd.get('name'), description: fd.get('description'), permission_scopes: [{ path: fd.get('path') || '/', permission }] };
    try { await api(`/api/admin/role/${id ? 'update' : 'create'}`, { body }); closeModal(); toast(t('success')); await renderAdminRoles(); }
    catch (error) { toast(error.message, 'err'); }
  }
  async function renderAdminMetas() {
    const data = await api('/api/admin/meta/list'); const rows = safeArray(data?.content);
    const body = rows.map(row => `<tr><td><strong>${esc(row.path)}</strong></td><td>${row.password ? `<span class="pill pill--warn">${esc(t('protected'))}</span>` : '—'}</td><td>${esc(row.hide || '—')}</td><td>${row.write ? `<span class="pill pill--ok">${esc(t('writeEnabled'))}</span>` : '—'}</td><td><div class="row-actions"><button type="button" class="btn btn--ghost btn--sm" data-act="meta-edit" data-id="${esc(row.id)}">${esc(t('edit'))}</button>${actionButton('meta-delete', t('delete'), row.id, true)}</div></td></tr>`).join('');
    $('adminPanelBody').innerHTML = `${panelToolbar(t('pathRules'), `<button type="button" class="btn" data-act="meta-new">${esc(t('addRule'))}</button>`)}${rows.length ? adminTable([t('path'), t('password'), t('hidePatterns'), t('write'), t('actions')], body) : stateBox(t('noRules'), '')}`;
  }
  async function metaDialog(id = 0) {
    let meta = null;
    if (id) { const data = await api('/api/admin/meta/list'); meta = safeArray(data?.content).find(row => Number(row.id) === Number(id)); }
    openModal(`<form data-form="meta" data-id="${esc(id)}">${modalHead(id ? t('editRule') : t('addRule'))}${field('path', t('path'), meta?.path || '/', 'text', { required: true })}${field('password', t('password'), meta?.password || '', 'password')}${checkbox('p_sub', t('passwordSubtree'), Boolean(meta?.p_sub))}${checkbox('write', t('writeEnabled'), Boolean(meta?.write))}${checkbox('w_sub', t('writeSubtree'), Boolean(meta?.w_sub))}${field('hide', t('hidePatterns'), meta?.hide || '', 'text')}${checkbox('h_sub', t('hideSubtree'), Boolean(meta?.h_sub))}${field('readme', t('readmeText'), meta?.readme || '', 'textarea')}${checkbox('r_sub', t('readmeSubtree'), Boolean(meta?.r_sub))}<div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="submit" class="btn btn--primary">${esc(t('save'))}</button></div></form>`, { wide: true });
  }
  async function saveMeta(form, fd) {
    const id = Number(form.dataset.id || 0); const body = { id, path: fd.get('path'), password: fd.get('password'), p_sub: fd.get('p_sub') === 'on', write: fd.get('write') === 'on', w_sub: fd.get('w_sub') === 'on', hide: fd.get('hide') || '', h_sub: fd.get('h_sub') === 'on', readme: fd.get('readme') || '', r_sub: fd.get('r_sub') === 'on' };
    try { await api(`/api/admin/meta/${id ? 'update' : 'create'}`, { body }); closeModal(); toast(t('success')); await renderAdminMetas(); }
    catch (error) { toast(error.message, 'err'); }
  }
  async function renderAdminShares() {
    const data = await api('/api/share/list'); const rows = safeArray(data?.content);
    const html = rows.map(row => `<tr><td><strong>${esc(row.path)}</strong></td><td>${esc(row.creator || '')}</td><td>${esc(row.views || 0)}</td><td>${esc(row.downloads || 0)}${row.max_downloads ? ` / ${esc(row.max_downloads)}` : ''}</td><td>${row.has_password ? `<span class="pill pill--warn">${esc(t('protected'))}</span>` : '—'}</td><td><div class="row-actions"><button type="button" class="btn btn--ghost btn--sm" data-act="copy-share-link" data-id="${esc(row.share_id)}">${esc(t('copyLink'))}</button>${actionButton('share-delete', t('deleteShare'), row.share_id, true)}</div></td></tr>`).join('');
    $('adminPanelBody').innerHTML = `${panelToolbar(t('shares'), `<button type="button" class="btn" data-act="refresh-admin">${esc(t('refreshNow'))}</button>`)}${rows.length ? adminTable([t('path'), t('owner'), t('views'), t('downloads'), t('password'), t('actions')], html) : stateBox(t('noSharesAdmin'), '')}`;
  }
  async function renderAdminTasks() {
    const [active, done] = await Promise.all([api('/api/task/undone'), api('/api/task/done')]);
    const rows = [...safeArray(active).map(item => ({ ...item, done: false })), ...safeArray(done).map(item => ({ ...item, done: true }))];
    const body = rows.map(row => `<tr><td><strong>${esc(row.name || row.id)}</strong><div class="field-help">${esc(row.creator || '')}</div></td><td>${esc(row.type)}</td><td><span class="pill ${row.done ? 'pill--ok' : 'pill--action'}">${esc(row.state || row.status)}</span></td><td><div class="progress-track"><div class="progress-fill" style="transform:scaleX(${Math.max(0, Math.min(1, Number(row.progress) || 0))})"></div></div>${Math.round((Number(row.progress) || 0) * 100)}%</td><td>${row.done ? '' : actionButton('task-cancel', t('cancel'), row.id, true)}</td></tr>`).join('');
    const actions = `<button type="button" class="btn" data-act="refresh-admin">${esc(t('refreshNow'))}</button><button type="button" class="btn btn--ghost" data-act="task-clear">${esc(t('clearCompleted'))}</button>`;
    $('adminPanelBody').innerHTML = `${panelToolbar(t('tasks'), actions)}${rows.length ? adminTable([t('name'), t('taskType'), t('status'), t('progress'), t('actions')], body) : stateBox(t('noAdminTasks'), '')}`;
  }
  const SETTING_FIELDS = [
    ['site_title', 'siteTitle', 'text'], ['announcement', 'announcementSetting', 'textarea'], ['default_page_size', 'pageSize', 'number'],
    ['pagination_type', 'pagination', 'select'], ['allow_indexed', 'allowIndexed', 'check'], ['allow_mounted', 'allowMounted', 'check'],
    ['robots_txt', 'robots', 'textarea'], ['enable_search', 'searchEnabled', 'check'], ['search_index', 'searchIndex', 'text'],
    ['sign_all', 'signAll', 'check'], ['link_expiration', 'linkExpiration', 'number'], ['max_devices', 'maxDevices', 'number'],
    ['device_session_ttl', 'sessionTtl', 'number'], ['forward_direct_link_params', 'forwardParams', 'check']
  ];
  function settingValue(data, key, defaultValue = '') { return data?.[key] ?? defaultValue; }
  async function renderAdminSettings() {
    const [settings, progress] = await Promise.all([api('/api/admin/setting/list'), api('/api/admin/index/progress')]);
    state.adminData.settings = settings;
    const fields = SETTING_FIELDS.map(([key, label, type]) => {
      const value = settingValue(settings, key, key === 'default_page_size' ? '50' : key === 'device_session_ttl' ? '86400' : '');
      if (type === 'check') return `<div class="field"><span class="field-label">${esc(t(label))}</span>${checkbox(key, t(label), value === true || value === 'true' || value === '1')}</div>`;
      if (type === 'select') return `<label class="field"><span class="field-label">${esc(t(label))}</span><select class="input" name="${esc(key)}"><option value="all" ${value === 'all' ? 'selected' : ''}>all</option><option value="pagination" ${value === 'pagination' ? 'selected' : ''}>pagination</option></select></label>`;
      return field(key, t(label), value, type === 'textarea' ? 'textarea' : type, { min: type === 'number' ? 0 : undefined });
    }).join('');
    const tools = `<div class="panel-toolbar"><h3 class="panel-heading">${esc(t('indexProgress'))}</h3><span class="pill pill--action">${esc(t('indexedCount', { n: progress?.obj_count || 0 }))}</span></div><div class="stage-actions"><button type="button" class="btn" data-act="index-build">${esc(t('buildIndex'))}</button><button type="button" class="btn btn--ghost" data-act="index-clear">${esc(t('clearIndex'))}</button><button type="button" class="btn btn--ghost" data-act="token-reset">${esc(t('resetToken'))}</button></div>`;
    $('adminPanelBody').innerHTML = `${panelToolbar(t('settings'))}<p class="stage-sub">${esc(t('settingsDesc'))}</p><form data-form="settings"><div class="form-grid">${fields}</div><div class="modal-foot"><button type="submit" class="btn btn--primary">${esc(t('saveSettings'))}</button></div></form><section class="surface-card admin-tool-card">${tools}</section>`;
  }
  async function saveSettings(form) {
    const fd = new FormData(form); const entries = [];
    for (const [key, label, type] of SETTING_FIELDS) {
      let value;
      if (type === 'check') value = fd.get(key) === 'on' ? 'true' : 'false';
      else value = String(fd.get(key) ?? '');
      entries.push({ key, value });
    }
    try { await api('/api/admin/setting/save', { body: entries }); toast(t('settingsSaved')); await renderAdminSettings(); }
    catch (error) { toast(error.message, 'err'); }
  }
  async function resetSystemToken() {
    try {
      const data = await api('/api/admin/setting/reset_token', { method: 'POST', body: {} });
      openModal(`${modalHead(t('resetToken'))}<p class="field-help">${esc(t('tokenReset'))}</p><pre class="code-view is-wrap" id="newSystemToken">${esc(data.token)}</pre><div class="modal-foot"><button type="button" class="btn btn--primary" data-act="copy-token">${esc(t('copyToken'))}</button></div>`);
      await renderAdminSettings();
    } catch (error) { toast(error.message, 'err'); }
  }
  async function actionAdmin(action, id = '') {
    try {
      if (action === 'storage-new') await storageDialog();
      else if (action === 'storage-edit') await storageDialog(id);
      else if (action === 'storage-delete') {
        const row = (await api('/api/admin/storage/list')).content?.find(item => String(item.id) === String(id));
        if (!row) return;
        openModal(`${modalHead(t('confirmDelete'), row.mount_path)}<div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="button" class="btn btn--danger" data-act="confirm-storage-delete" data-id="${esc(id)}">${esc(t('delete'))}</button></div>`);
      } else if (action === 'storage-reload') { await api('/api/admin/storage/load_all', { method: 'POST', body: {} }); toast(t('success')); await renderAdminStorages(); }
      else if (action === 'user-new') await userDialog();
      else if (action === 'user-edit') await userDialog(id);
      else if (action === 'guest-toggle') await toggleGuest();
      else if (action === 'role-new') roleDialog();
      else if (action === 'role-edit') { const data = await api('/api/admin/role/list'); roleDialog(safeArray(data?.content).find(item => String(item.id) === String(id))); }
      else if (action === 'meta-new') await metaDialog();
      else if (action === 'meta-edit') await metaDialog(id);
      else if (action === 'refresh-admin') await loadAdminPanel();
    } catch (error) { toast(error.message, 'err'); }
  }

  async function copyText(text) {
    try {
      await navigator.clipboard.writeText(String(text)); toast(t('copied')); return;
    } catch {}
    const input = document.createElement('textarea'); input.value = String(text); input.style.position = 'fixed'; input.style.opacity = '0';
    document.body.appendChild(input); input.select();
    try { document.execCommand('copy'); toast(t('copied')); } catch { toast(String(text), 'info', 8000); }
    input.remove();
  }
  async function copyShareLink(id) { await copyText(`${location.origin}/s/${encodeURIComponent(id)}`); }
  function askAdminDelete(kind, id) {
    const title = kind === 'user' ? t('deleteUser') : kind === 'role' ? t('deleteRole') : kind === 'share' ? t('deleteShare') : t('delete');
    openModal(`${modalHead(t('confirmDelete'), title)}<div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="button" class="btn btn--danger" data-act="confirm-admin-delete" data-kind="${esc(kind)}" data-id="${esc(id)}">${esc(t('delete'))}</button></div>`);
  }
  async function doAdminDelete(kind, id) {
    const routes = { user: 'user', role: 'role', meta: 'meta' };
    try {
      if (kind === 'share') await api('/api/share/delete', { body: { share_id: id } });
      else if (routes[kind]) await api(`/api/admin/${routes[kind]}/delete?id=${encodeURIComponent(id)}`, { method: 'POST', body: {} });
      else if (kind === 'storage') await api(`/api/admin/storage/delete?id=${encodeURIComponent(id)}`, { method: 'POST', body: {} });
      closeModal(); toast(t('success')); await loadAdminPanel();
    } catch (error) { toast(error.message, 'err'); }
  }
  async function authAction() {
    if (!state.user) { openLogin(); return; }
    try { await api('/api/auth/logout', { method: 'POST', body: {} }); } catch {}
    state.user = null; state.token = ''; removeStore(STORAGE_KEY); updateAuthButton(); renderSidebar(); closeModal(); toast(t('signOut')); await loadDir('/');
  }
  async function clickAction(action, element, event) {
    const path = element.dataset.path || '';
    const id = element.dataset.id || '';
    if (action === 'toggle-lang') { state.lang = state.lang === 'en' ? 'vi' : 'en'; writeStore('kova.language', state.lang); applyLanguage(); }
    else if (action === 'toggle-theme') setTheme(state.theme === 'system' ? 'light' : state.theme === 'light' ? 'dark' : 'system');
    else if (action === 'toggle-sidebar') { const open = !$('appSidebar').classList.contains('is-open'); $('appSidebar').classList.toggle('is-open', open); $('sidebarScrim').hidden = !open; $('btnMobileNav').setAttribute('aria-expanded', String(open)); }
    else if (action === 'close-sidebar') closeSidebar();
    else if (action === 'go-home') { if (state.currentView === 'share') { history.pushState({}, '', '/'); } await loadDir('/'); }
    else if (action === 'navigate') await loadDir(path);
    else if (action === 'open-admin') openAdmin();
    else if (action === 'close-admin') await loadDir(state.path || '/');
    else if (action === 'auth-action') { if (state.user) openAccount(); else openLogin(); }
    else if (action === 'sign-out') await authAction();
    else if (action === 'open-account') openAccount();
    else if (action === 'open-palette') openPalette();
    else if (action === 'close-palette') closePalette();
    else if (action === 'palette-backdrop' && event.target === $('paletteOverlay')) closePalette();
    else if (action === 'palette-scope') {
      state.palette.scope = Number(element.dataset.scope || 0);
      $('paletteScopes').querySelectorAll('.scope-chip').forEach(chip => chip.classList.toggle('is-active', chip === element));
      schedulePaletteSearch();
    } else if (action === 'palette-result') await choosePaletteResult(Number(element.dataset.index));
    else if (action === 'dismiss-announcement' && !state.backendUnavailable) { $('announcementBar').hidden = true; state.announcementDismissed = true; writeStore('kova.notice.dismissed', '1'); }
    else if (action === 'set-view') setView(element.dataset.view);
    else if (action === 'refresh-dir') await loadDir(state.path, true);
    else if (action === 'load-more') { state.renderLimit = Math.min(state.filtered.length, state.renderLimit + 160); renderExplorer(); }
    else if (action === 'sort') { const key = element.dataset.sort; state.sort = { key, dir: state.sort.key === key ? state.sort.dir * -1 : 1 }; renderExplorer(); }
    else if (action === 'trigger-upload') $('fileInput').click();
    else if (action === 'trigger-folder-upload') $('folderInput').click();
    else if (action === 'new-folder') openCreateFolder();
    else if (action === 'new-file') openCreateFile();
    else if (action === 'offline-dl') openOfflineDialog();
    else if (action === 'toggle-readme-mode') { state.readmeRaw = !state.readmeRaw; renderExplorer(); }
    else if (action === 'item-open') {
      if (event.target.closest('input,button')) return;
      state.focusIndex = Number(element.dataset.index || 0); await openItem(element.dataset.itemPath || path);
    } else if (action === 'toggle-select') { event.stopPropagation(); await toggleItemSelection(path); }
    else if (action === 'select-all') { if (state.selected.size === state.filtered.length) state.selected.clear(); else for (const item of state.filtered) state.selected.add(itemFullPath(item)); renderExplorer(); }
    else if (action === 'clear-selection') { state.selected.clear(); renderExplorer(); }
    else if (action === 'batch-copy') await doBatchMoveCopy('copy');
    else if (action === 'batch-move') await doBatchMoveCopy('move');
    else if (action === 'batch-delete') openDeleteConfirm([...state.selected]);
    else if (action === 'confirm-delete') await removeNames(state.pendingDelete || []);
    else if (action === 'item-download') await downloadPath(path);
    else if (action === 'item-menu') { const rect = element.getBoundingClientRect(); showContext(path, rect.right, rect.bottom); }
    else if (action === 'ctx-open') { const target = state.contextPath; hideContext(); await openItem(target); }
    else if (action === 'ctx-download') { const target = state.contextPath; hideContext(); await downloadPath(target); }
    else if (action === 'ctx-share') { const target = state.contextPath; hideContext(); openShareDialog(target); }
    else if (action === 'ctx-rename') { const target = state.contextPath; hideContext(); openRename(target); }
    else if (action === 'ctx-delete') { const target = state.contextPath; hideContext(); openDeleteConfirm([target]); }
    else if (action === 'close-modal') closeModal();
    else if (action === 'modal-backdrop' && event.target === $('modalOverlay')) closeModal();
    else if (action === 'copy-text') await copyText(element.dataset.text || '');
    else if (action === 'copy-token') await copyText($('newSystemToken')?.textContent || '');
    else if (action === 'picker-open') await loadPickerPath(path);
    else if (action === 'picker-parent') await loadPickerPath(parentPath(state.picker?.path || '/'));
    else if (action === 'picker-choose') await pickerChoose();
    else if (action === 'archive-download') await downloadArchiveEntry(Number(element.dataset.index));
    else if (action === 'archive-extract') openPicker('archive');
    else if (action === 'edit-preview') await editPreview();
    else if (action === 'save-preview') {
      try { await saveTextFile(state.previewPath, $('previewEditor').value); closeModal(); toast(t('saved')); await loadDir(state.path, true); }
      catch (error) { toast(error.message, 'err'); }
    } else if (action === 'cancel-edit') await openPreview(state.previewPath, state.previewShare, state.previewItem);
    else if (action === 'download-current') await downloadPath(state.previewPath, state.previewShare);
    else if (action === 'zoom-image') element.classList.toggle('is-zoomed');
    else if (action === 'file-properties') {
      const file = state.items.find(item => itemFullPath(item) === path);
      openModal(`${modalHead(t('fileDetails'))}<dl class="property-list"><dt>${esc(t('name'))}</dt><dd>${esc(file?.name || path.split('/').at(-1))}</dd><dt>${esc(t('path'))}</dt><dd>${esc(path)}</dd><dt>${esc(t('size'))}</dt><dd>${esc(formatBytes(file?.size))}</dd><dt>${esc(t('modified'))}</dt><dd>${esc(formatDate(file?.modified))}</dd><dt>${esc(t('provider'))}</dt><dd>${esc(state.listMeta?.provider || t('unknown'))}</dd></dl>`);
    } else if (action === 'show-shortcuts') showShortcuts();
    else if (action === 'setup-2fa') await setup2fa();
    else if (action === 'disable-2fa') showDisable2fa();
    else if (action === 'change-password') showChangePassword();
    else if (action === 'account-sessions') await showSessions();
    else if (action === 'account-shares') await showUserShares();
    else if (action === 'account-tasks') await showUserTasks();
    else if (action === 'revoke-session') {
      try { await api('/api/me/sessions/evict', { body: { device_key: element.dataset.key } }); toast(t('success')); await showSessions(); }
      catch (error) { toast(error.message, 'err'); }
    } else if (action === 'copy-share-link') await copyShareLink(id);
    else if (action === 'admin-tab') selectAdminTab(element.dataset.tab);
    else if (action === 'telemetry-toggle') { state.telemetryPaused = !state.telemetryPaused; await loadAdminPanel(); }
    else if (action.startsWith('storage-') || action.startsWith('user-') || action.startsWith('role-') || action.startsWith('meta-')) {
      if (action === 'user-delete' || action === 'role-delete' || action === 'meta-delete') askAdminDelete(action.split('-')[0], id);
      else if (action === 'user-reset-2fa') openModal(`${modalHead(t('reset2fa'))}<p class="field-help">${esc(t('confirmDeleteDesc'))}</p><div class="modal-foot"><button type="button" class="btn" data-act="close-modal">${esc(t('cancelAction'))}</button><button type="button" class="btn btn--danger" data-act="confirm-reset-2fa" data-id="${esc(id)}">${esc(t('reset2fa'))}</button></div>`);
      else await actionAdmin(action, id);
    } else if (action === 'confirm-admin-delete') await doAdminDelete(element.dataset.kind, id);
    else if (action === 'confirm-storage-delete') await doAdminDelete('storage', id);
    else if (action === 'confirm-reset-2fa') {
      try { await api(`/api/admin/user/cancel_2fa?id=${encodeURIComponent(id)}`, { method: 'POST', body: {} }); closeModal(); toast(t('success')); await renderAdminUsers(); }
      catch (error) { toast(error.message, 'err'); }
    } else if (action === 'guest-toggle') await toggleGuest();
    else if (action === 'share-delete') askAdminDelete('share', id);
    else if (action === 'task-cancel') {
      try { await api(`/api/task/cancel?tid=${encodeURIComponent(id)}`, { method: 'POST', body: {} }); toast(t('success')); await renderAdminTasks(); }
      catch (error) { toast(error.message, 'err'); }
    } else if (action === 'task-clear') {
      try { await api('/api/task/clear_done', { method: 'POST', body: {} }); toast(t('success')); await renderAdminTasks(); }
      catch (error) { toast(error.message, 'err'); }
    } else if (action === 'storage-reload') await actionAdmin(action);
    else if (action === 'index-build') {
      element.disabled = true;
      try { const data = await api('/api/admin/index/build', { method: 'POST', body: {} }); toast(t('indexedCount', { n: data.indexed })); await renderAdminSettings(); }
      catch (error) { toast(error.message, 'err'); } finally { element.disabled = false; }
    } else if (action === 'index-clear') {
      try { await api('/api/admin/index/clear', { method: 'POST', body: {} }); toast(t('success')); await renderAdminSettings(); }
      catch (error) { toast(error.message, 'err'); }
    } else if (action === 'token-reset') await resetSystemToken();
    else if (action === 'share-download') await doDownloadShare();
    else if (action === 'share-preview') await openPreview('/', true, state.share);
    else if (action === 'share-navigate') { state.shareSubpath = path; await loadShareDir(path); }
    else if (action === 'share-item-open') {
      if (event.target.closest('button')) return;
      const item = state.shareItems[Number(element.dataset.index)]; await previewShareItem(element.dataset.itemPath || path, item);
    } else if (action === 'share-item-preview') { const item = state.shareItems[Number(element.dataset.index)]; await previewShareItem(path, item); }
    else if (action === 'close-upload-dock') { $('uploadDock').hidden = true; state.uploadQueue = []; renderUploadQueue(); }
    else if (action === 'cancel-upload') cancelUploads();
    else if (action === 'admin-open') openAdmin();
  }
  function showShortcuts() {
    const rows = [
      ['shortcutSearch', '⌘ K'], ['shortcutFilter', '/'], ['shortcutMove', '↑  ↓'], ['shortcutOpen', 'Enter'], ['shortcutParent', 'Backspace'],
      ['shortcutSelect', 'Space'], ['shortcutRename', 'F2'], ['shortcutDelete', 'Delete'], ['shortcutHelp', '?']
    ];
    openModal(`${modalHead(t('shortcuts'))}<div class="kbd-table">${rows.map(([label, key]) => `<div class="kbd-row"><span>${esc(t(label))}</span><kbd>${esc(key)}</kbd></div>`).join('')}</div>`);
  }
  function onClick(event) {
    const element = event.target.closest('[data-act]');
    if (!element) {
      if (!event.target.closest('#ctxMenu')) hideContext();
      return;
    }
    const action = element.dataset.act;
    if (element.tagName === 'BUTTON' || element.tagName === 'A') event.preventDefault();
    Promise.resolve(clickAction(action, element, event)).catch(error => {
      if (error?.name !== 'AbortError') toast(error?.message || t('operationFailed'), 'err');
    });
  }
  function onSubmit(event) {
    const form = event.target.closest('form[data-form]'); if (!form) return;
    const formType = form.dataset.form;
    if (formType === 'share-auth') { event.preventDefault(); submitShareAuth(form, new FormData(form)); }
    else submitForm(form, event);
  }
  function onInput(event) {
    if (event.target.id === 'filterInput') { state.filter = event.target.value; state.renderLimit = 160; renderExplorer(); }
    else if (event.target.id === 'paletteInput') schedulePaletteSearch();
  }
  function onChange(event) {
    if (event.target.id === 'paletteGlobal') { state.palette.global = event.target.checked; schedulePaletteSearch(); }
    else if (event.target.id === 'fileInput' || event.target.id === 'folderInput') {
      const files = Array.from(event.target.files || []);
      enqueueFiles(files.map(file => ({ file, relativePath: file.webkitRelativePath || file.name })));
      event.target.value = '';
    } else if (event.target.id === 'storageDriver') renderDriverFields(event.target.value);
  }
  function onKeyDown(event) {
    const target = event.target;
    const editable = target.matches('input, textarea, select, [contenteditable="true"]');
    if ((event.metaKey || event.ctrlKey) && event.key.toLowerCase() === 'k') { event.preventDefault(); if ($('paletteOverlay').hidden) openPalette(); else closePalette(); return; }
    if (!$('paletteOverlay').hidden) {
      if (event.key === 'Escape') { event.preventDefault(); closePalette(); }
      else if (event.key === 'ArrowDown' || event.key === 'ArrowUp') {
        event.preventDefault(); const max = state.palette.results.length - 1;
        if (max >= 0) { state.palette.index = (state.palette.index + (event.key === 'ArrowDown' ? 1 : -1) + state.palette.results.length) % state.palette.results.length; renderPalette(state.palette.results.filter(item => item.kind === 'file')); }
      } else if (event.key === 'Enter') { event.preventDefault(); choosePaletteResult(state.palette.index); }
      return;
    }
    if (!$('modalOverlay').hidden) {
      if (event.key === 'Escape') { event.preventDefault(); closeModal(); }
      if (event.key === 'Tab') {
        const focusables = Array.from($('modalContent').querySelectorAll('button:not([disabled]),input:not([disabled]),select:not([disabled]),textarea:not([disabled]),a[href]')).filter(el => el.offsetParent !== null);
        if (focusables.length) {
          const first = focusables[0], last = focusables.at(-1);
          if (event.shiftKey && document.activeElement === first) { event.preventDefault(); last.focus(); }
          else if (!event.shiftKey && document.activeElement === last) { event.preventDefault(); first.focus(); }
        }
      }
      return;
    }
    if (event.key === 'Escape') { hideContext(); if (state.selected.size) { state.selected.clear(); renderExplorer(); } return; }
    if (event.key === '?' && !editable) { event.preventDefault(); showShortcuts(); return; }
    if (event.key === '/' && !editable) { event.preventDefault(); $('filterInput').focus(); return; }
    if (editable) return;
    if (event.key === 'Backspace') { event.preventDefault(); if (state.currentView === 'explorer' && state.path !== '/') loadDir(parentPath(state.path)); return; }
    if (event.key === 'ArrowDown') { event.preventDefault(); setFocused(state.focusIndex < 0 ? 0 : state.focusIndex + 1); return; }
    if (event.key === 'ArrowUp') { event.preventDefault(); setFocused(state.focusIndex < 0 ? 0 : state.focusIndex - 1); return; }
    if (event.key === 'Enter' && state.focusIndex >= 0) { event.preventDefault(); const item = state.filtered[state.focusIndex]; if (item) openItem(itemFullPath(item)); return; }
    if (event.code === 'Space' && state.focusIndex >= 0) { event.preventDefault(); const item = state.filtered[state.focusIndex]; if (item) toggleItemSelection(itemFullPath(item)); return; }
    if (event.key === 'F2') { event.preventDefault(); if (state.selected.size === 1) openRename([...state.selected][0]); else if (state.focusIndex >= 0 && state.filtered[state.focusIndex]) openRename(itemFullPath(state.filtered[state.focusIndex])); return; }
    if (event.key === 'Delete' && state.selected.size) { event.preventDefault(); openDeleteConfirm([...state.selected]); }
  }
  function onContextMenu(event) {
    const row = event.target.closest('[data-item-path]');
    if (!row || state.currentView !== 'explorer') return;
    event.preventDefault();
    const path = row.dataset.itemPath;
    if (!state.selected.has(path)) { state.selected.clear(); state.selected.add(path); renderExplorer(); }
    showContext(path, event.clientX, event.clientY);
  }
  let dragDepth = 0;
  function onDragEnter(event) { if (!event.dataTransfer?.types?.includes('Files')) return; event.preventDefault(); dragDepth++; $('dropOverlay').hidden = false; }
  function onDragOver(event) { if (event.dataTransfer?.types?.includes('Files')) { event.preventDefault(); event.dataTransfer.dropEffect = state.canWrite ? 'copy' : 'none'; } }
  function onDragLeave(event) { if (!event.dataTransfer?.types?.includes('Files')) return; dragDepth = Math.max(0, dragDepth - 1); if (!dragDepth) $('dropOverlay').hidden = true; }
  async function onDrop(event) {
    if (!event.dataTransfer?.types?.includes('Files')) return;
    event.preventDefault(); dragDepth = 0; $('dropOverlay').hidden = true;
    if (!state.canWrite) { toast(t('noWrite'), 'err'); return; }
    const entries = await getDroppedFiles(event.dataTransfer); enqueueFiles(entries);
  }
  function handlePopState() {
    const match = location.pathname.match(/^\/s\/([^/]+)\/?$/);
    if (match) loadShare(decodeURIComponent(match[1])); else loadDir('/');
  }
  document.addEventListener('click', onClick);
  document.addEventListener('submit', onSubmit);
  document.addEventListener('input', onInput);
  document.addEventListener('change', onChange);
  document.addEventListener('keydown', onKeyDown);
  document.addEventListener('contextmenu', onContextMenu);
  document.addEventListener('dragenter', onDragEnter);
  document.addEventListener('dragover', onDragOver);
  document.addEventListener('dragleave', onDragLeave);
  document.addEventListener('drop', onDrop);
  window.addEventListener('popstate', handlePopState);
  window.addEventListener('resize', () => {
    const tab = $('adminTabs')?.querySelector(`[data-tab="${state.adminTab}"]`);
    if (tab) { const indicator = $('adminTabIndicator'); indicator.style.width = `${tab.offsetWidth}px`; indicator.style.transform = `translateX(${tab.offsetLeft - 4}px)`; }
  }, { passive: true });
  setTheme(state.theme);
  applyLanguage();
  loadInitial();
})();
