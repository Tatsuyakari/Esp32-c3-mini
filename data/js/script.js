document.addEventListener('DOMContentLoaded', function() {
  // Elements
  const scanBtn = document.getElementById('scan-btn');
  const networkList = document.getElementById('network-list');
  const connectForm = document.getElementById('connect-form');
  const ssidInput = document.getElementById('ssid');
  const passInput = document.getElementById('pass');
  const disconnectBtn = document.getElementById('disconnect-btn');
  const connectionStatus = document.getElementById('connection-status');
  const apInfo = document.getElementById('ap-info');
  const wifiInfo = document.getElementById('wifi-info');
  
  // ESP-NOW elements
  const espnowModeDisplay = document.getElementById('espnow-mode-display');
  const espnowManager = document.getElementById('espnow-manager');
  const masterControls = document.getElementById('master-controls');
  const slaveInfo = document.getElementById('slave-info');
  const scanEspnowBtn = document.getElementById('scan-espnow-btn');
  const espnowDeviceList = document.getElementById('espnow-device-list');
  const deviceMac = document.getElementById('device-mac');
  const apSsidDisplay = document.getElementById('ap-ssid-display');
  
  // Slave LED elements
  const slaveLedIndicator = document.getElementById('slave-led-indicator');
  const slaveLedStatus = document.getElementById('slave-led-status');
  const toggleSlaveLedBtn = document.getElementById('toggle-slave-led');
  const connectionNote = document.getElementById('connection-note');
  
  // Biến lưu trạng thái
  let isMaster = true;
  let espnowActive = false;
  let slaveLedState = false;
  let slaveConnectedToMaster = false;
  
  // Fetch initial status
  fetchStatus();
  
  // Event listeners
  scanBtn.addEventListener('click', scanNetworks);
  connectForm.addEventListener('submit', connectToWifi);
  disconnectBtn.addEventListener('click', disconnectWifi);
  scanEspnowBtn && scanEspnowBtn.addEventListener('click', scanEspnowDevices);
  toggleSlaveLedBtn && toggleSlaveLedBtn.addEventListener('click', toggleSlaveLed);
  
  // Thiết lập sự kiện cho các nút động
  document.addEventListener('click', function(e) {
    // Xử lý nút kết nối ESP-NOW
    if (e.target.closest('.connect-btn')) {
      const btn = e.target.closest('.connect-btn');
      const mac = btn.getAttribute('data-mac');
      if (mac) connectToEspnowDevice(mac);
    }
    
    // Xử lý nút toggle LED
    if (e.target.closest('.toggle-btn')) {
      const btn = e.target.closest('.toggle-btn');
      const mac = btn.getAttribute('data-mac');
      if (mac) toggleLed(mac);
    }
  });
  
  // Functions
  function fetchStatus() {
    fetch('/api/status')
      .then(response => response.json())
      .then(data => {
        // Update connection status
        connectionStatus.innerHTML = data.wifi_connected ? 
          '<i class="fas fa-check-circle"></i> Đã kết nối' : 
          '<i class="fas fa-times-circle"></i> Chưa kết nối';
        
        // Update AP info
        apInfo.textContent = `${data.ap_ssid} (${data.ap_ip})`;
        
        // Hiển thị SSID ở phần chế độ ESP-NOW
        if (apSsidDisplay) {
          apSsidDisplay.textContent = data.ap_ssid;
          // Thêm màu sắc phù hợp theo chế độ
          apSsidDisplay.style.color = data.is_master ? 'var(--primary-color)' : 'var(--secondary-color)';
        }
        
        // Update WiFi connection info
        if (data.wifi_connected) {
          wifiInfo.innerHTML = `<span style="color: var(--success-color);">${data.wifi_ssid}</span> (${data.wifi_ip})`;
          disconnectBtn.style.display = 'inline-block';
        } else {
          wifiInfo.innerHTML = '<span style="color: var(--danger-color);">Chưa kết nối</span>';
          disconnectBtn.style.display = 'none';
        }
        
        // Update ESP-NOW status
        isMaster = data.is_master;
        espnowActive = data.espnow_active;
        
        // Update ESP-NOW mode display
        espnowModeDisplay.innerHTML = isMaster ? 
          '<span style="color: var(--primary-color);">Master</span>' : 
          '<span style="color: var(--secondary-color);">Slave</span>';
        
        // Show/hide relevant ESP-NOW controls
        if (espnowActive) {
          espnowManager.style.display = 'block';
          if (isMaster) {
            masterControls.style.display = 'block';
            slaveInfo.style.display = 'none';
            fetchEspnowDevices(); // Tự động tải danh sách thiết bị
          } else {
            masterControls.style.display = 'none';
            slaveInfo.style.display = 'block';
            
            // Get MAC address and LED state for slave mode display
            fetch('/api/espnow/list')
              .then(response => response.json())
              .then(data => {
                if (data.devices && data.devices.length > 0) {
                  deviceMac.textContent = data.devices[0].mac;
                  
                  // Update LED state for slave
                  slaveLedState = data.devices[0].ledState;
                  slaveConnectedToMaster = data.devices[0].connected;
                  updateSlaveLedDisplay();
                } else {
                  deviceMac.textContent = 'Không có sẵn';
                  slaveLedState = false;
                  slaveConnectedToMaster = false;
                  updateSlaveLedDisplay();
                }
              })
              .catch(error => {
                deviceMac.textContent = 'Không có sẵn';
                slaveLedState = false;
                slaveConnectedToMaster = false;
                updateSlaveLedDisplay();
              });
          }
        } else {
          espnowManager.style.display = 'none';
        }
      })
      .catch(error => {
        showToast('Không thể tải trạng thái: ' + error.message);
      });
  }
  
  function updateSlaveLedDisplay() {
    if (!slaveLedIndicator || !slaveLedStatus) return;
    
    if (!slaveConnectedToMaster) {
      // Khi chưa kết nối với master, LED nhấp nháy
      slaveLedIndicator.className = 'led-indicator led-blink';
      slaveLedStatus.textContent = 'Đang nhấp nháy (Chưa kết nối với Master)';
      connectionNote.style.display = 'block';
      toggleSlaveLedBtn.disabled = true;
    } else {
      // Khi đã kết nối, hiển thị trạng thái LED
      connectionNote.style.display = 'none';
      toggleSlaveLedBtn.disabled = false;
      
      if (slaveLedState) {
        slaveLedIndicator.className = 'led-indicator led-on';
        slaveLedStatus.textContent = 'BẬT';
        toggleSlaveLedBtn.innerHTML = '<i class="fas fa-power-off"></i> Tắt LED';
        toggleSlaveLedBtn.className = 'btn success';
      } else {
        slaveLedIndicator.className = 'led-indicator led-off';
        slaveLedStatus.textContent = 'TẮT';
        toggleSlaveLedBtn.innerHTML = '<i class="fas fa-lightbulb"></i> Bật LED';
        toggleSlaveLedBtn.className = 'btn primary';
      }
    }
  }
  
  function toggleSlaveLed() {
    if (isMaster) return; // Chỉ slave mới có thể tự điều khiển LED
    
    // Hiệu ứng nút nhấn
    toggleSlaveLedBtn.disabled = true;
    toggleSlaveLedBtn.innerHTML = '<i class="fas fa-circle-notch fa-spin"></i> Đang xử lý...';
    
    // Gửi yêu cầu toggle LED
    fetch('/api/espnow/slave/toggle', {
      method: 'POST',
      body: new FormData()
    })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          slaveLedState = !slaveLedState; // Đảo trạng thái
          updateSlaveLedDisplay();
          showToast('Đã ' + (slaveLedState ? 'BẬT' : 'TẮT') + ' LED');
        } else {
          showToast('Lỗi: ' + data.message);
        }
        toggleSlaveLedBtn.disabled = false;
      })
      .catch(error => {
        showToast('Lỗi: ' + error.message);
        toggleSlaveLedBtn.disabled = false;
      });
  }
  
  function scanNetworks() {
    scanBtn.disabled = true;
    scanBtn.innerHTML = '<i class="fas fa-circle-notch fa-spin"></i> Đang quét...';
    networkList.innerHTML = '<div class="network-item" style="text-align: center;"><i class="fas fa-circle-notch fa-spin"></i> Đang tìm kiếm mạng WiFi...</div>';
    
    fetch('/api/scan')
      .then(response => response.json())
      .then(data => {
        networkList.innerHTML = '';
        
        if (data.networks && data.networks.length > 0) {
          data.networks.forEach(network => {
            const item = createNetworkItem(network);
            item.addEventListener('click', () => {
              ssidInput.value = network.ssid;
              passInput.focus();
            });
            networkList.appendChild(item);
          });
        } else {
          networkList.innerHTML = '<div class="network-item" style="text-align: center; color: #666;"><i class="fas fa-exclamation-circle"></i> Không tìm thấy mạng WiFi nào</div>';
        }
        
        scanBtn.disabled = false;
        scanBtn.innerHTML = '<i class="fas fa-search"></i> Quét WiFi';
      })
      .catch(error => {
        networkList.innerHTML = '<div class="network-item" style="text-align: center; color: #f44336;"><i class="fas fa-exclamation-triangle"></i> Lỗi: ' + error.message + '</div>';
        scanBtn.disabled = false;
        scanBtn.innerHTML = '<i class="fas fa-search"></i> Quét WiFi';
        showToast('Lỗi: ' + error.message);
      });
  }
  
  function connectToWifi(event) {
    event.preventDefault();
    
    const ssid = ssidInput.value.trim();
    const pass = passInput.value;
    
    if (!ssid) {
      showToast('Vui lòng nhập tên mạng WiFi');
      return;
    }
    
    const submitBtn = connectForm.querySelector('button[type="submit"]');
    submitBtn.disabled = true;
    submitBtn.innerHTML = '<i class="fas fa-circle-notch fa-spin"></i> Đang kết nối...';
    
    const formData = new FormData();
    formData.append('ssid', ssid);
    formData.append('pass', pass);
    
    fetch('/api/connect', {
      method: 'POST',
      body: formData
    })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          showToast('Đang kết nối đến ' + ssid);
          setTimeout(fetchStatus, 5000); // Kiểm tra trạng thái sau 5 giây
        } else {
          showToast('Lỗi: ' + data.message);
        }
        submitBtn.disabled = false;
        submitBtn.innerHTML = '<i class="fas fa-sign-in-alt"></i> Kết nối';
      })
      .catch(error => {
        showToast('Lỗi: ' + error.message);
        submitBtn.disabled = false;
        submitBtn.innerHTML = '<i class="fas fa-sign-in-alt"></i> Kết nối';
      });
  }
  
  function disconnectWifi() {
    disconnectBtn.disabled = true;
    disconnectBtn.innerHTML = '<i class="fas fa-circle-notch fa-spin"></i> Đang ngắt kết nối...';
    
    fetch('/api/disconnect', {
      method: 'POST',
      body: new FormData()
    })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          showToast('Đã ngắt kết nối WiFi');
          setTimeout(fetchStatus, 2000); // Kiểm tra trạng thái sau 2 giây
        } else {
          showToast('Lỗi: ' + data.message);
        }
        disconnectBtn.disabled = false;
        disconnectBtn.innerHTML = '<i class="fas fa-unlink"></i> Ngắt kết nối';
      })
      .catch(error => {
        showToast('Lỗi: ' + error.message);
        disconnectBtn.disabled = false;
        disconnectBtn.innerHTML = '<i class="fas fa-unlink"></i> Ngắt kết nối';
      });
  }
  
  function scanEspnowDevices() {
    if (!isMaster || !espnowActive) return;
    
    scanEspnowBtn.disabled = true;
    scanEspnowBtn.innerHTML = '<i class="fas fa-circle-notch fa-spin"></i> Đang quét...';
    
    fetch('/api/espnow/scan', {
      method: 'POST',
      body: new FormData()
    })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          showToast('Đang quét thiết bị ESP-NOW');
          setTimeout(fetchEspnowDevices, 2000); // Tải danh sách sau 2 giây
        } else {
          showToast('Lỗi: ' + data.message);
        }
        scanEspnowBtn.disabled = false;
        scanEspnowBtn.innerHTML = '<i class="fas fa-search"></i> Quét thiết bị ESP-NOW';
      })
      .catch(error => {
        showToast('Lỗi: ' + error.message);
        scanEspnowBtn.disabled = false;
        scanEspnowBtn.innerHTML = '<i class="fas fa-search"></i> Quét thiết bị ESP-NOW';
      });
  }
  
  function fetchEspnowDevices() {
    if (!isMaster || !espnowActive) return;
    
    fetch('/api/espnow/list')
      .then(response => response.json())
      .then(data => {
        espnowDeviceList.innerHTML = '';
        
        if (data.devices && data.devices.length > 0) {
          data.devices.forEach(device => {
            const item = createDeviceItem(device);
            espnowDeviceList.appendChild(item);
          });
        } else {
          espnowDeviceList.innerHTML = '<div class="network-item" style="text-align: center; color: #666;"><i class="fas fa-info-circle"></i> Chưa có thiết bị nào. Nhấn nút Quét để tìm thiết bị.</div>';
        }
      })
      .catch(error => {
        espnowDeviceList.innerHTML = '<div class="network-item" style="text-align: center; color: #f44336;"><i class="fas fa-exclamation-triangle"></i> Lỗi: ' + error.message + '</div>';
      });
  }
  
  function connectToEspnowDevice(mac) {
    if (!isMaster || !espnowActive) return;
    
    const formData = new FormData();
    formData.append('mac', mac);
    
    fetch('/api/espnow/connect', {
      method: 'POST',
      body: formData
    })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          showToast('Đã kết nối với thiết bị');
          setTimeout(fetchEspnowDevices, 1000); // Tải lại danh sách sau 1 giây
        } else {
          showToast('Lỗi: ' + data.message);
        }
      })
      .catch(error => {
        showToast('Lỗi: ' + error.message);
      });
  }
  
  function toggleLed(mac) {
    if (!isMaster || !espnowActive) return;
    
    const formData = new FormData();
    formData.append('mac', mac);
    
    fetch('/api/espnow/toggle', {
      method: 'POST',
      body: formData
    })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          showToast('Đã gửi lệnh toggle LED');
          setTimeout(fetchEspnowDevices, 1000); // Tải lại danh sách sau 1 giây
        } else {
          showToast('Lỗi: ' + data.message);
        }
      })
      .catch(error => {
        showToast('Lỗi: ' + error.message);
      });
  }
  
  // Auto-refresh status
  setInterval(fetchStatus, 10000); // Cập nhật trạng thái mỗi 10 giây
  
  // Auto-refresh ESP-NOW device list when in Master mode
  setInterval(() => {
    if (isMaster && espnowActive) {
      fetchEspnowDevices();
    }
  }, 5000); // Cập nhật danh sách thiết bị mỗi 5 giây
  
  function createNetworkItem(network) {
    const item = document.createElement('div');
    item.className = 'network-item';
    
    // Calculate signal strength (0-4)
    const signalStrength = calculateSignalStrength(network.rssi);
    const signalBars = '▂▄▆█'.slice(0, signalStrength);
    
    item.innerHTML = `
      <div class="network-icon">${network.secure ? '<i class="fas fa-lock"></i>' : '<i class="fas fa-unlock"></i>'}</div>
      <div class="network-details">
        <div class="network-name">${network.ssid}</div>
        <div class="network-info">${network.secure ? 'Bảo mật' : 'Mở'}</div>
      </div>
      <div class="signal-strength">
        <span class="signal-bars">${signalBars}</span>
        <span>${network.rssi} dBm</span>
      </div>
    `;
    
    return item;
  }
  
  function createDeviceItem(device) {
    const item = document.createElement('div');
    item.className = 'network-item';
    
    // Calculate signal strength (0-4)
    const signalStrength = calculateSignalStrength(device.rssi);
    const signalBars = '▂▄▆█'.slice(0, signalStrength);
    
    item.innerHTML = `
      <div class="network-icon">${device.connected ? '<i class="fas fa-plug"></i>' : '<i class="fas fa-unlink"></i>'}</div>
      <div class="network-details">
        <div class="network-name">${device.name}</div>
        <div class="network-info">${device.mac}</div>
      </div>
      <div class="signal-strength">
        <span class="signal-bars">${signalBars}</span>
        <span>${device.rssi} dBm</span>
      </div>
    `;
    
    if (!device.connected) {
      // Add connect button
      const connectBtn = document.createElement('button');
      connectBtn.className = 'btn primary connect-btn';
      connectBtn.style.marginLeft = '10px';
      connectBtn.textContent = 'Kết nối';
      connectBtn.setAttribute('data-mac', device.mac);
      item.appendChild(connectBtn);
    } else {
      // Add toggle LED button
      const toggleBtn = document.createElement('button');
      toggleBtn.className = 'btn ' + (device.ledState ? 'success' : 'secondary') + ' toggle-btn';
      toggleBtn.style.marginLeft = '10px';
      toggleBtn.textContent = device.ledState ? 'Tắt LED' : 'Bật LED';
      toggleBtn.setAttribute('data-mac', device.mac);
      item.appendChild(toggleBtn);
    }
    
    return item;
  }
  
  function calculateSignalStrength(rssi) {
    // RSSI thường từ -100dBm (yếu) đến -30dBm (mạnh)
    if (rssi >= -50) return 4;      // Rất mạnh (-50 đến -30)
    else if (rssi >= -65) return 3; // Mạnh (-65 đến -50)
    else if (rssi >= -75) return 2; // Trung bình (-75 đến -65)
    else if (rssi >= -85) return 1; // Yếu (-85 đến -75)
    else return 0;                  // Rất yếu (dưới -85)
  }
  
  function showToast(message) {
    const toast = document.getElementById('toast');
    toast.textContent = message;
    toast.classList.add('show');
    
    setTimeout(() => {
      toast.classList.remove('show');
    }, 3000);
  }
});