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
  
  // Biến lưu trạng thái
  let isMaster = true;
  let espnowActive = false;
  
  // Fetch initial status
  fetchStatus();
  
  // Event listeners
  scanBtn.addEventListener('click', scanNetworks);
  connectForm.addEventListener('submit', connectToWifi);
  disconnectBtn.addEventListener('click', disconnectWifi);
  scanEspnowBtn && scanEspnowBtn.addEventListener('click', scanEspnowDevices);
  
  // Functions
  function fetchStatus() {
    fetch('/api/status')
      .then(response => response.json())
      .then(data => {
        // Update AP info
        apInfo.textContent = `${data.ap_ssid} (${data.ap_ip})`;
        // Hiển thị SSID ở phần chế độ ESP-NOW
        if (apSsidDisplay) {
          apSsidDisplay.textContent = data.ap_ssid;
          // Thêm màu sắc phù hợp theo chế độ
          apSsidDisplay.style.color = data.is_master ? '#1976d2' : '#f44336';
        }
        
        // Update WiFi connection info
        if (data.wifi_connected) {
          connectionStatus.textContent = 'Đã kết nối';
          connectionStatus.style.color = '#4caf50';
          wifiInfo.textContent = `${data.wifi_ssid} (${data.wifi_ip})`;
          disconnectBtn.style.display = 'block';
        } else {
          connectionStatus.textContent = 'Chưa kết nối';
          connectionStatus.style.color = '#f44336';
          wifiInfo.textContent = 'Chưa kết nối';
          disconnectBtn.style.display = 'none';
        }
        
        // Update ESP-NOW status
        isMaster = data.is_master;
        espnowActive = data.espnow_active;
        
        // Update ESP-NOW mode display
        espnowModeDisplay.textContent = isMaster ? 'Master' : 'Slave';
        
        // Show/hide relevant ESP-NOW controls
        if (espnowActive) {
          espnowManager.style.display = 'block';
          if (isMaster) {
            masterControls.style.display = 'block';
            slaveInfo.style.display = 'none';
          } else {
            masterControls.style.display = 'none';
            slaveInfo.style.display = 'block';
            // Get MAC address for slave mode display
            fetch('/api/espnow/list')
              .then(response => response.json())
              .then(data => {
                if (data.devices && data.devices.length > 0) {
                  deviceMac.textContent = data.devices[0].mac;
                } else {
                  deviceMac.textContent = 'Không có sẵn';
                }
              })
              .catch(error => {
                deviceMac.textContent = 'Không có sẵn';
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
  
  function scanNetworks() {
    scanBtn.disabled = true;
    scanBtn.textContent = 'Đang quét...';
    networkList.innerHTML = '<p>Đang tìm kiếm mạng WiFi...</p>';
    
    fetch('/api/scan')
      .then(response => response.json())
      .then(data => {
        networkList.innerHTML = '';
        
        if (data.networks && data.networks.length > 0) {
          data.networks.forEach(network => {
            const networkItem = document.createElement('div');
            networkItem.className = 'network-item';
            networkItem.addEventListener('click', () => {
              ssidInput.value = network.ssid;
              passInput.focus();
            });
            
            // Calculate signal strength (0-4)
            const signalStrength = calculateSignalStrength(network.rssi);
            const signalBars = '▂▄▆█'.slice(0, signalStrength);
            
            networkItem.innerHTML = `
              <div class="network-icon">${network.secure ? '🔒' : '🔓'}</div>
              <div class="network-details">
                <div class="network-name">${network.ssid}</div>
                <div class="network-info">${network.secure ? 'Bảo mật' : 'Mở'}</div>
              </div>
              <div class="signal-strength">
                <span class="signal-bars">${signalBars}</span>
                <span>${network.rssi} dBm</span>
              </div>
            `;
            
            networkList.appendChild(networkItem);
          });
        } else {
          networkList.innerHTML = '<p>Không tìm thấy mạng WiFi nào</p>';
        }
      })
      .catch(error => {
        networkList.innerHTML = '<p>Lỗi khi quét: ' + error.message + '</p>';
      })
      .finally(() => {
        scanBtn.disabled = false;
        scanBtn.textContent = 'Quét WiFi';
      });
  }
  
  function connectToWifi(event) {
    event.preventDefault();
    
    const formData = new FormData(connectForm);
    const ssid = formData.get('ssid');
    const pass = formData.get('pass');
    
    if (!ssid) {
      showToast('Vui lòng nhập SSID');
      return;
    }
    
    const submitBtn = connectForm.querySelector('button[type="submit"]');
    submitBtn.disabled = true;
    submitBtn.textContent = 'Đang kết nối...';
    
    fetch('/api/connect', {
      method: 'POST',
      body: formData
    })
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          showToast('Kết nối thành công!');
          fetchStatus();
        } else {
          showToast('Kết nối thất bại: ' + data.message);
        }
      })
      .catch(error => {
        showToast('Lỗi: ' + error.message);
      })
      .finally(() => {
        submitBtn.disabled = false;
        submitBtn.textContent = 'Kết nối';
      });
  }
  
  function disconnectWifi() {
    disconnectBtn.disabled = true;
    disconnectBtn.textContent = 'Đang ngắt kết nối...';
    
    fetch('/api/disconnect')
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          showToast('Đã ngắt kết nối WiFi');
          fetchStatus();
        } else {
          showToast(data.message);
        }
      })
      .catch(error => {
        showToast('Lỗi: ' + error.message);
      })
      .finally(() => {
        disconnectBtn.disabled = false;
        disconnectBtn.textContent = 'Ngắt kết nối';
      });
  }
  
  // Chức năng chuyển đổi chế độ đã bị loại bỏ vì chế độ được xác định khi biên dịch
  
  function scanEspnowDevices() {
    // Kiểm tra nếu scanEspnowBtn không tồn tại (trường hợp thiết bị slave)
    if (!scanEspnowBtn) return;
    
    scanEspnowBtn.disabled = true;
    scanEspnowBtn.textContent = 'Đang quét...';
    espnowDeviceList.innerHTML = '<p>Đang tìm kiếm thiết bị ESP-NOW...</p>';
    
    fetch('/api/espnow/scan')
      .then(response => response.json())
      .then(data => {
        if (data.success) {
          // Wait 5 seconds to give ESP devices time to respond
          setTimeout(fetchEspnowDevices, 5000);
        } else {
          espnowDeviceList.innerHTML = '<p>Lỗi: ' + data.message + '</p>';
          scanEspnowBtn.disabled = false;
          scanEspnowBtn.textContent = 'Quét thiết bị ESP-NOW';
        }
      })
      .catch(error => {
        espnowDeviceList.innerHTML = '<p>Lỗi khi quét: ' + error.message + '</p>';
        scanEspnowBtn.disabled = false;
        scanEspnowBtn.textContent = 'Quét thiết bị ESP-NOW';
      });
  }
  
  function fetchEspnowDevices() {
    fetch('/api/espnow/list')
      .then(response => response.json())
      .then(data => {
        espnowDeviceList.innerHTML = '';
        
        if (data.devices && data.devices.length > 0) {
          data.devices.forEach(device => {
            const deviceItem = document.createElement('div');
            deviceItem.className = 'network-item';
            
            // Calculate signal strength (0-4)
            const signalStrength = calculateSignalStrength(device.rssi);
            const signalBars = '▂▄▆█'.slice(0, signalStrength);
            
            deviceItem.innerHTML = `
              <div class="network-icon">${device.connected ? '🔌' : '📡'}</div>
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
              connectBtn.className = 'btn primary';
              connectBtn.style.marginLeft = '10px';
              connectBtn.textContent = 'Kết nối';
              connectBtn.addEventListener('click', () => connectToEspnowDevice(device.mac));
              deviceItem.appendChild(connectBtn);
            } else {
              // Add toggle LED button
              const toggleBtn = document.createElement('button');
              toggleBtn.className = 'btn ' + (device.ledState ? 'success' : 'secondary');
              toggleBtn.style.marginLeft = '10px';
              toggleBtn.textContent = device.ledState ? 'Tắt LED' : 'Bật LED';
              toggleBtn.addEventListener('click', () => toggleLed(device.mac, !device.ledState));
              deviceItem.appendChild(toggleBtn);
            }
            
            espnowDeviceList.appendChild(deviceItem);
          });
        } else {
          espnowDeviceList.innerHTML = '<p>Không tìm thấy thiết bị ESP-NOW nào</p>';
        }
        
        scanEspnowBtn.disabled = false;
        scanEspnowBtn.textContent = 'Quét thiết bị ESP-NOW';
      })
      .catch(error => {
        espnowDeviceList.innerHTML = '<p>Lỗi khi lấy danh sách: ' + error.message + '</p>';
        scanEspnowBtn.disabled = false;
        scanEspnowBtn.textContent = 'Quét thiết bị ESP-NOW';
      });
  }
  
  function connectToEspnowDevice(mac) {
    const formData = new FormData();
    formData.append('mac', mac);
    
    fetch('/api/espnow/connect', {
      method: 'POST',
      body: formData
    })
      .then(response => response.json())
      .then(data => {
        showToast(data.message);
        if (data.success) {
          // Refresh device list after successful connection
          setTimeout(fetchEspnowDevices, 500);
        }
      })
      .catch(error => {
        showToast('Lỗi: ' + error.message);
      });
  }
  
  function toggleLed(mac, state) {
    const formData = new FormData();
    formData.append('mac', mac);
    formData.append('state', state ? '1' : '0');
    
    fetch('/api/espnow/toggle', {
      method: 'POST',
      body: formData
    })
      .then(response => response.json())
      .then(data => {
        showToast(data.message);
        if (data.success) {
          // Refresh device list after toggling LED
          setTimeout(fetchEspnowDevices, 500);
        }
      })
      .catch(error => {
        showToast('Lỗi: ' + error.message);
      });
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
  
  // Auto-refresh ESP-NOW device list when in Master mode
  setInterval(() => {
    if (isMaster && espnowActive) {
      fetchEspnowDevices();
    }
  }, 5000); // Refresh every 5 seconds để phát hiện thiết bị slave nhanh hơn
});