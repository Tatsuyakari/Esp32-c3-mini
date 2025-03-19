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
    
    // Fetch initial status
    fetchStatus();
    
    // Event listeners
    scanBtn.addEventListener('click', scanNetworks);
    connectForm.addEventListener('submit', connectToWifi);
    disconnectBtn.addEventListener('click', disconnectWifi);
    
    // Functions
    function fetchStatus() {
      fetch('/api/status')
        .then(response => response.json())
        .then(data => {
          // Update AP info
          apInfo.textContent = `${data.ap_ssid} (${data.ap_ip})`;
          
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
  