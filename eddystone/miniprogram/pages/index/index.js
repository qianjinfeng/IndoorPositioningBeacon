//index.js
const app = getApp()

function inArray(arr, key, val) {
  for (let i = 0; i < arr.length; i++) {
    if (arr[i][key] === val) {
      return i;
    }
  }
  return -1;
}

// ArrayBuffer转16进度字符串示例
function ab2hex(buffer) {
  var hexArr = Array.prototype.map.call(
    new Uint8Array(buffer),
    function (bit) {
      return ('00' + bit.toString(16)).slice(-2)
    }
  )
  return hexArr.join('');
}

Page({
  data: {
    info: "未初始化蓝牙适配器",
    devices: []
  },

  startBluetoothDevicesDiscovery() {
    if (this._discoveryStarted) {
      return
    }
    this._discoveryStarted = true
    wx.startBluetoothDevicesDiscovery({
      services: ['FEAA'],
      allowDuplicatesKey: false,
      success: (res) => {
        console.log('startBluetoothDevicesDiscovery success', res)
        this.onBluetoothDeviceFound()
      },
    })
  },

  onBluetoothDeviceFound() {
    wx.onBluetoothDeviceFound((res) => {
      console.log('onBluetoothDeviceFound success', res)
      res.devices.forEach(device => {
        if (device.advertisServiceUUIDs.length > 0) {
          //'0000FEAA-0000-1000-8000-00805F9B34FB'
          //console.log(Object.getOwnPropertyDescriptor(device.serviceData, device.advertisServiceUUIDs[0]))
          var eddystone = Object.getOwnPropertyDescriptor(device.serviceData, device.advertisServiceUUIDs[0])
          console.log(ab2hex(eddystone))
          var type = new Uint8Array(eddystone.value);
          device.type = type[0]
          if (type[0] == 0) {
            device.asset = ab2hex(eddystone.value.slice(12))
          }
          else if (type[0] == 32) {
            var dataView = new DataView(eddystone.value)
            console.log(dataView.getUint16(1))
            device.battery = dataView.getUint16(1)
          }
        }
        const foundDevices = this.data.devices
        const idx = inArray(foundDevices, 'deviceId', device.deviceId)
        const data = {}
        if (idx === -1) {
          data[`devices[${foundDevices.length}]`] = device
        } else {
          data[`devices[${idx}]`] = device
        }

        this.setData(data)
      })
    })
  },

  onHide: function () {
    wx.stopBluetoothDevicesDiscovery()
  },
  onUnload: function () {
    wx.closeBluetoothAdapter()
    this._discoveryStarted = false
  },
  onShow: function () {
    this.startBluetoothDevicesDiscovery()
  },
  onLoad: function() {
    var that = this;
    wx.openBluetoothAdapter({
      success: function (res) {
        console.log('初始化蓝牙适配器成功')
        //页面日志显示
        that.setData({
          info: '初始化蓝牙适配器成功,开始扫描'
        })
        that.startBluetoothDevicesDiscovery()
      },
      fail: function (res) {
        if (res.errCode === 10001) {
          wx.onBluetoothAdapterStateChange(function (res) {
            console.log('onBluetoothAdapterStateChange', res)
            if (res.available) {
              that.startBluetoothDevicesDiscovery()
            }
          })
        }
        else {
          console.log('请打开蓝牙和定位功能')
          that.setData({
            info: '请打开蓝牙和定位功能'
          })
        }
      }
    })

  },

})
