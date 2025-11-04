import sys
import numpy as np
import pyqtgraph as pg
pg.setConfigOptions(antialias=True)
from pyqtgraph.Qt import QtCore, QtWidgets, QtGui
import serial
import serial.tools.list_ports
from collections import deque
import traceback
import numpy.fft as fft 

class SerialConfigDialog(QtWidgets.QDialog):
    def __init__(self, current_params, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Configuración de Puerto Serial")
        self.setMinimumWidth(350)

        self.stopbits_mapping = {
            serial.STOPBITS_ONE: '1',
            serial.STOPBITS_ONE_POINT_FIVE: '1.5',
            serial.STOPBITS_TWO: '2'
        }
        self.parity_mapping = {
            serial.PARITY_NONE: 'Ninguno',
            serial.PARITY_EVEN: 'Par',
            serial.PARITY_ODD: 'Impar',
            serial.PARITY_MARK: 'Marca',
            serial.PARITY_SPACE: 'Espacio'
        }
        self.bytesize_mapping = {
             serial.FIVEBITS: '5', serial.SIXBITS: '6',
             serial.SEVENBITS: '7', serial.EIGHTBITS: '8'
        }
        self.reverse_stopbits = {v: k for k, v in self.stopbits_mapping.items()}
        self.reverse_parity = {v: k for k, v in self.parity_mapping.items()}
        self.reverse_bytesize = {v: k for k, v in self.bytesize_mapping.items()}

        layout = QtWidgets.QVBoxLayout()
        form_layout = QtWidgets.QFormLayout()

        self.port_combo = QtWidgets.QComboBox()
        self.refresh_ports(current_params.get('port'))
        self.btn_refresh = QtWidgets.QPushButton("Actualizar")
        self.btn_refresh.clicked.connect(lambda: self.refresh_ports(self.port_combo.currentData()))

        self.baud_combo = QtWidgets.QComboBox()
        baud_rates = ['4800', '9600', '19200', '38400', '57600', '115200']
        self.baud_combo.addItems(baud_rates)
        current_baud_val = current_params.get('baudrate', 115200)
        current_baud_text = str(current_baud_val)
        index = self.baud_combo.findText(current_baud_text)
        if index >= 0:
            self.baud_combo.setCurrentIndex(index)
        else:
            default_index = self.baud_combo.findText('115200')
            if default_index >= 0: self.baud_combo.setCurrentIndex(default_index)

        self.data_bits = QtWidgets.QComboBox()
        self.data_bits.addItems(self.bytesize_mapping.values())
        current_bytesize_val = current_params.get('bytesize', serial.EIGHTBITS)
        current_bytesize_text = self.bytesize_mapping.get(current_bytesize_val, '8')
        self.data_bits.setCurrentText(current_bytesize_text)

        self.stop_bits = QtWidgets.QComboBox()
        self.stop_bits.addItems(self.stopbits_mapping.values())
        current_stopbits_val = current_params.get('stopbits', serial.STOPBITS_ONE)
        current_stopbits_text = self.stopbits_mapping.get(current_stopbits_val, '1')
        self.stop_bits.setCurrentText(current_stopbits_text)

        self.parity = QtWidgets.QComboBox()
        self.parity.addItems(self.parity_mapping.values())
        current_parity_val = current_params.get('parity', serial.PARITY_NONE)
        current_parity_text = self.parity_mapping.get(current_parity_val, 'Ninguno')
        self.parity.setCurrentText(current_parity_text)

        current_timeout_val = current_params.get('timeout', 0.05)
        timeout_ms = int(current_timeout_val * 1000)
        self.timeout = QtWidgets.QLineEdit(str(timeout_ms))
        self.timeout.setValidator(QtGui.QIntValidator(0, 60000, self))

        port_layout = QtWidgets.QHBoxLayout()
        port_layout.addWidget(self.port_combo, 1)
        port_layout.addWidget(self.btn_refresh)
        form_layout.addRow("Puerto:", port_layout)
        form_layout.addRow("Baud Rate:", self.baud_combo)
        form_layout.addRow("Bits de Datos:", self.data_bits)
        form_layout.addRow("Bits de Parada:", self.stop_bits)
        form_layout.addRow("Paridad:", self.parity)
        form_layout.addRow("Timeout (ms):", self.timeout)

        btn_box = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Ok |
            QtWidgets.QDialogButtonBox.StandardButton.Cancel
        )
        btn_box.accepted.connect(self.accept)
        btn_box.rejected.connect(self.reject)

        layout.addLayout(form_layout)
        layout.addWidget(btn_box)
        self.setLayout(layout)

    def refresh_ports(self, current_port_device=None):
        self.port_combo.clear()
        ports = []
        try:
            ports = serial.tools.list_ports.comports()
        except Exception as e:
             QtWidgets.QMessageBox.critical(self, "Error de Puertos", f"No se pudieron listar los puertos seriales:\n{e}")

        if not ports:
            self.port_combo.addItem("No hay puertos disponibles")
            self.port_combo.setEnabled(False)
            return

        self.port_combo.setEnabled(True)
        selected_index = 0
        for i, port in enumerate(ports):
            display_text = f"{port.device} ({port.description or 'N/A'})"
            self.port_combo.addItem(display_text, port.device)
            if current_port_device and port.device == current_port_device:
                selected_index = i

        self.port_combo.setCurrentIndex(selected_index)

    def get_config(self):
        selected_port_device = self.port_combo.currentData()
        selected_baud_text = self.baud_combo.currentText()
        selected_databits_text = self.data_bits.currentText()
        selected_stopbits_text = self.stop_bits.currentText()
        selected_parity_text = self.parity.currentText()
        timeout_text = self.timeout.text()

        if selected_port_device is None:
             QtWidgets.QMessageBox.warning(self, "Error de Configuración", "Seleccione un puerto serial válido.")
             return None

        try:
            timeout_val_ms = int(timeout_text)
            if timeout_val_ms < 0: timeout_val_ms = 0
            timeout_val_sec = timeout_val_ms / 1000.0
        except ValueError:
             QtWidgets.QMessageBox.warning(self, "Error de Configuración", f"El valor de Timeout ('{timeout_text}') no es un número válido en milisegundos.")
             return None

        try:
            config = {
                'port': selected_port_device,
                'baudrate': int(selected_baud_text),
                'bytesize': self.reverse_bytesize[selected_databits_text],
                'stopbits': self.reverse_stopbits[selected_stopbits_text],
                'parity': self.reverse_parity[selected_parity_text],
                'timeout': timeout_val_sec
            }
            return config
        except KeyError as e:
            msg = f"Error interno al mapear valor de configuración: Clave '{e}' no encontrada. Verifique mapeos y textos UI."
            QtWidgets.QMessageBox.critical(self, "Error Interno", msg)
            return None
        except Exception as e:
             msg = f"Ocurrió un error inesperado al obtener la configuración:\n{str(e)}"
             QtWidgets.QMessageBox.critical(self, "Error Inesperado", msg)
             return None

class ChannelConfigDialog(QtWidgets.QDialog):
    def __init__(self, current_params, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Configuración de Canal")
        self.setMinimumWidth(400)

        layout = QtWidgets.QVBoxLayout()
        form_layout = QtWidgets.QFormLayout()

        # --- Canal (0-7) ---
        self.channel = QtWidgets.QSpinBox()
        self.channel.setRange(0, 7)

        # --- Tipo de Señal (0-2) 
        self.signal_type = QtWidgets.QComboBox()
        self.signal_type.addItems(["Nativa", "Rectificada", "Envolvente"])  

        # --- Ganancia (0-2) ---
        self.gain = QtWidgets.QComboBox()
        self.gain.addItems(["10", "25", "50"])

        # --- Filtro Pasa Bajos (0-2) ---
        self.lowpass = QtWidgets.QComboBox()
        self.lowpass.addItems(["15 Hz", "20 Hz", "25 Hz"]) 

        # --- Filtro Pasa Altos (0-2) ---
        self.highpass = QtWidgets.QComboBox()
        self.highpass.addItems(["150 Hz", "350 Hz", "400 Hz"])  

        # --- Cargar valores iniciales ---
        self.channel.setValue(current_params.get("channel", 0))
        self.signal_type.setCurrentIndex(current_params.get("signal_type", 0))
        self.gain.setCurrentIndex(current_params.get("gain", 0))
        self.lowpass.setCurrentIndex(current_params.get("lowpass", 0))
        self.highpass.setCurrentIndex(current_params.get("highpass", 0))

        # --- Diseño ---
        form_layout.addRow("Canal (0-7):", self.channel)
        form_layout.addRow("Tipo de Señal:", self.signal_type)
        form_layout.addRow("Ganancia:", self.gain)
        form_layout.addRow("Filtro P. Bajos:", self.lowpass)
        form_layout.addRow("Filtro P. Altos:", self.highpass)

        # --- Botones (Guardar/Cerrar) ---
        btn_box = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Save |
            QtWidgets.QDialogButtonBox.StandardButton.Close
        )
        btn_box.accepted.connect(self.accept)
        btn_box.rejected.connect(self.reject)

        layout.addLayout(form_layout)
        layout.addWidget(btn_box)
        self.setLayout(layout)

    def get_config(self):
        # Mapea todas las opciones a valores 0-2 (o 0-7 para el canal)
        return {
            "channel": self.channel.value(),
            "signal_type": self.signal_type.currentIndex(),
            "gain": self.gain.currentIndex(),
            "lowpass": self.lowpass.currentIndex(),
            "highpass": self.highpass.currentIndex()
        }

class RealTimePlot(QtWidgets.QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("SISTEMA MULTICANAL DE ELECTROMIOGRAFÍA")
        central_widget = QtWidgets.QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QtWidgets.QHBoxLayout(central_widget)
        control_layout = QtWidgets.QHBoxLayout()

        self.channel_states = [
        {'configured': False, 'tipo': None, 'gain': None, 'lp': None, 'hp': None}
        for _ in range(8)
        ]

        # --- Panel Izquierdo ---
        left_panel = QtWidgets.QGroupBox("Configuración Actual")
        left_panel.setStyleSheet("""
        QGroupBox { border: 2px solid #606060; border-radius: 5px; margin-top: 1ex; font-weight: bold; padding: 10px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; color: #404040; }
        """)
        left_layout = QtWidgets.QVBoxLayout(left_panel)
        main_layout.addWidget(left_panel, stretch=1)

        # contenedor vertical para 8 cajas
        self.channel_cards = []     # lista de QFrames por canal 0..7
        self.channel_titles = []    # titulo canal

        for ch in range(8):
            card = QtWidgets.QFrame()
            card.setFrameShape(QtWidgets.QFrame.Shape.Box)   # PyQt6
            card.setStyleSheet("QFrame { border: 1px solid #888; border-radius: 6px; }")
            v = QtWidgets.QVBoxLayout(card)

            title = QtWidgets.QLabel(f"Canal {ch}")
            title.setStyleSheet("font-weight: 600;")
            v.addWidget(title)

            # Tooltip inicial
            card.setToolTip("Canal no configurado")
            title.setToolTip("Canal no configurado")

            left_layout.addWidget(card)
            self.channel_cards.append(card)
            self.channel_titles.append(title)

        left_layout.addStretch(1)

        for ch in range(8):
            self.channel_states[ch] = {
                'configured': False, 'tipo': None, 'gain': None, 'lp': None, 'hp': None
            }
            self.channel_cards[ch].setToolTip("Canal no configurado")
            self.channel_titles[ch].setToolTip("Canal no configurado")
            self.channel_cards[ch].setStyleSheet("QFrame { border: 1px solid #888; border-radius: 6px; }")

        # --- Panel Derecho (Controles + Gráficas) ---
        right_panel = QtWidgets.QWidget()
        right_layout = QtWidgets.QVBoxLayout()
        right_panel.setLayout(right_layout)
        main_layout.addWidget(right_panel, stretch=8)

        # --- Controles Superiores ---
        control_layout = QtWidgets.QHBoxLayout() 
        self.btn_config = QtWidgets.QPushButton("Configuración Serial", self)
        self.btn_config.clicked.connect(self.show_config_dialog)
        control_layout.addWidget(self.btn_config)

        self.btn_channel_config = QtWidgets.QPushButton("Config. Canal", self)
        self.btn_channel_config.clicked.connect(self._show_channel_config_dialog)
        control_layout.addWidget(self.btn_channel_config)

        self.btn_connect = QtWidgets.QPushButton("Conectar", self)
        self.btn_connect.clicked.connect(self.toggle_connection)
        control_layout.addWidget(self.btn_connect)

        self.connection_indicator = QtWidgets.QLabel()
        self.connection_indicator.setFixedSize(20, 20)
        self.connection_indicator.setStyleSheet("background-color: red; border-radius: 10px;")
        control_layout.addWidget(self.connection_indicator)

        self.status_label = QtWidgets.QLabel("Desconectado")
        control_layout.addWidget(self.status_label)
        control_layout.addStretch()

                # --- Controles de FFT ---
        self.fft_ch_combo = QtWidgets.QComboBox()
        self.fft_ch_combo.addItems([f"Canal {i}" for i in range(8)])
        control_layout.addWidget(self.fft_ch_combo)

        self.btn_fft = QtWidgets.QPushButton("FFT")
        self.btn_fft.setToolTip("Abrir ventana de FFT del canal seleccionado (se pueden abrir varias).")
        self.btn_fft.clicked.connect(self._on_fft_open_clicked)
        control_layout.addWidget(self.btn_fft)

        right_layout.addLayout(control_layout) 

        # --- Rejilla dinámica para las gráficas ---
        self.plots_grid = QtWidgets.QGridLayout()
        right_layout.addLayout(self.plots_grid, stretch=2)

        self.plots_grid.setColumnStretch(0, 1)
        self.plots_grid.setColumnStretch(1, 1)
        self.plots_grid.setHorizontalSpacing(8)
        self.plots_grid.setVerticalSpacing(8)

        self._plots_rows_used = 0  

        # Crear las 8 gráficas
        self.plot_chA = pg.PlotWidget(title="Canal 0"); self.curve_chA = self.plot_chA.plot(pen=pg.mkPen('y', width=2))
        self.plot_chB = pg.PlotWidget(title="Canal 1"); self.curve_chB = self.plot_chB.plot(pen=pg.mkPen('y', width=2))
        self.plot_chC = pg.PlotWidget(title="Canal 2"); self.curve_chC = self.plot_chC.plot(pen=pg.mkPen('y', width=2))
        self.plot_chD = pg.PlotWidget(title="Canal 3"); self.curve_chD = self.plot_chD.plot(pen=pg.mkPen('y', width=2))
        self.plot_chE = pg.PlotWidget(title="Canal 4"); self.curve_chE = self.plot_chE.plot(pen=pg.mkPen('y', width=2))
        self.plot_chF = pg.PlotWidget(title="Canal 5"); self.curve_chF = self.plot_chF.plot(pen=pg.mkPen('y', width=2))
        self.plot_chG = pg.PlotWidget(title="Canal 6"); self.curve_chG = self.plot_chG.plot(pen=pg.mkPen('y', width=2))
        self.plot_chH = pg.PlotWidget(title="Canal 7"); self.curve_chH = self.plot_chH.plot(pen=pg.mkPen('y', width=2))

        for c in [self.curve_chA, self.curve_chB, self.curve_chC, self.curve_chD,
                self.curve_chE, self.curve_chF, self.curve_chG, self.curve_chH]:
            c.setClipToView(True)
            # Cuando haya muchos puntos en pantalla, pyqtgraph “subsamplea” para que no se serruche
            c.setDownsampling(auto=True, method='subsample')

        # Configuración común de ejes
        for pw in [self.plot_chA, self.plot_chB, self.plot_chC, self.plot_chD, self.plot_chE, self.plot_chF, self.plot_chG, self.plot_chH]:
            pw.setLabel('left', 'Voltaje (V)', units='V')
            pw.setLabel('bottom', 'Tiempo (s)')
            pw.showGrid(x=True, y=True)
            pw.hide()  # Ocultas al inicio

        # Lista para manejar dinámicamente
        self.plot_widgets = [
            self.plot_chA, self.plot_chB, self.plot_chC, self.plot_chD,
            self.plot_chE, self.plot_chF, self.plot_chG, self.plot_chH
]


        # Parámetros de conversión
        self.v_ref = 3.3
        self.max_adc = 4095
        self.points_to_show = 50
        self.upsample_factor = 4
        self.scale_factor = 1.0
        self.display_offset_volts = 0.0
        self.smooth_enabled = False
        self.smooth_window  = 7    

        from collections import deque
        self.dataA = deque(maxlen=self.points_to_show)
        self.dataB = deque(maxlen=self.points_to_show)
        self.dataC = deque(maxlen=self.points_to_show)
        self.dataD = deque(maxlen=self.points_to_show)
        self.dataE = deque(maxlen=self.points_to_show)
        self.dataF = deque(maxlen=self.points_to_show)
        self.dataG = deque(maxlen=self.points_to_show)
        self.dataH = deque(maxlen=self.points_to_show)

        # Buffer para ensamblar frames del puerto serie
        self.buffer = bytearray()
        self.FRAME_HDR = b'\xA5\x5A'


        # Inicializar Parámetros
        self.channel_params = { "channel": 0, "gain": 0, "lowpass": 0, "highpass": 0, "signal_type": 0 }
        initial_port = None
        try:
            available_ports = serial.tools.list_ports.comports()
            if available_ports: initial_port = available_ports[0].device
        except Exception as e: print(f"[ERROR] Error al detectar puerto inicial: {e}")

        self.serial_params = { 'port': initial_port, 'baudrate': 115200, 'bytesize': serial.EIGHTBITS, 'stopbits': serial.STOPBITS_ONE, 'parity': serial.PARITY_NONE, 'timeout': 0.05 }
        self.ser = None
        self.connected = False


        # Timer
        self.timer = QtCore.QTimer()
        self.timer.setInterval(50) 
        self.timer.timeout.connect(self.update_plot)

        self.update_status_label()
        self._update_channel_labels()
    

        self.sampling_rate = 600
        self._apply_plot_limits()

        # --- FFT 
        self.fft_windows = {} 
        self._fft_refresh_ms = 200

        #  timer 
        self._fft_timer = QtCore.QTimer(self)
        self._fft_timer.timeout.connect(self._refresh_all_ffts)
        self._fft_timer.start(self._fft_refresh_ms)

        self.logo_izq = QtWidgets.QLabel(self)
        pixmap_izq = QtGui.QPixmap(u"C:/Users/57323/Downloads/logo-ub-b.png")
        pixmap_izq = pixmap_izq.scaled(110, 110, QtCore.Qt.AspectRatioMode.KeepAspectRatio)
        self.logo_izq.setPixmap(pixmap_izq)
        self.logo_izq.setFixedSize(110, 110)
        self.logo_izq.setAlignment(QtCore.Qt.AlignmentFlag.AlignLeft | QtCore.Qt.AlignmentFlag.AlignTop)


        self.logo_izq.raise_()  

    def resizeEvent(self, event):
        super().resizeEvent(event)
        # Logo inferior izquierdo
        self.logo_izq.move(30, self.height() - self.logo_izq.height() - 150)  


    def update_status_label(self):

        if self.connected:
            port = self.serial_params.get('port', 'N/A')
            baud = self.serial_params.get('baudrate', 'N/A')
            self.status_label.setText(f"Conectado a {port} ({baud})")
        else:
            port = self.serial_params.get('port', 'N/A')
            baud = self.serial_params.get('baudrate', 'N/A')
            status = "Desconectado."
            if port:
                 status = f"Listo para conectar a {port} ({baud})."
            elif not port and self.serial_params.get('port') is None:
                 status = "Seleccione un puerto en Configuración."

            self.status_label.setText(status)

    def show_config_dialog(self):

        dialog = SerialConfigDialog(self.serial_params, self)
        dialog_result = dialog.exec()

        if dialog_result == QtWidgets.QDialog.DialogCode.Accepted:
            new_config = dialog.get_config()

            if new_config is None:
                 return

            if new_config != self.serial_params:
                was_connected = self.connected

                if was_connected:
                    self._disconnect_serial()

                self.serial_params = new_config

                if was_connected:
                    self._connect_serial()
                else:
                    self.update_status_label()

    def toggle_connection(self):

        if not self.connected:
            self._connect_serial()
        else:
            self._disconnect_serial()

    def _connect_serial(self):

        if not self.serial_params.get('port'):
             QtWidgets.QMessageBox.warning(self, "Advertencia", "No se ha seleccionado ningún puerto serial.\nVaya a Configuración para seleccionar uno.")
             self.update_status_label()
             return

        if self.ser and self.ser.is_open:
            try: self.ser.close()
            except Exception as e: print(f"[WARN] [RealTimePlot] Advertencia: Error al cerrar puerto existente: {e}")

        try:
            self.ser = serial.Serial(**self.serial_params)

            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()

            # Limpia buffers para el nuevo framing de 2 canales
            self.buffer.clear()
            self.dataA.clear()
            self.dataB.clear()
            self.dataC.clear()
            self.dataD.clear()
            self.dataE.clear()
            self.dataF.clear()
            self.dataG.clear()
            self.dataH.clear()


            self.connected = True
            self.btn_connect.setText("Desconectar")
            self.connection_indicator.setStyleSheet("background-color: green; border-radius: 10px;")

            self.curve_chA.clear()
            self.curve_chB.clear()
            self.curve_chC.clear()
            self.curve_chD.clear()
            self.curve_chE.clear()
            self.curve_chF.clear()
            self.curve_chG.clear()
            self.curve_chH.clear()
    

            self.timer.start()


        except serial.SerialException as e:
            self.connected = False; self.ser = None
            error_msg = f"No se pudo conectar a {self.serial_params.get('port', 'N/A')}:\n{str(e)}"
            QtWidgets.QMessageBox.critical(self, "Error de Conexión", error_msg)
            self.connection_indicator.setStyleSheet("background-color: red; border-radius: 10px;")
            self.btn_connect.setText("Conectar"); self.timer.stop()
        except (TypeError, ValueError) as e:
             self.connected = False; self.ser = None
             error_msg = f"Parámetros seriales inválidos ({type(e).__name__}):\n{str(e)}\n\nVerifique la configuración."
             QtWidgets.QMessageBox.critical(self, "Error de Parámetros", error_msg)
             self.connection_indicator.setStyleSheet("background-color: red; border-radius: 10px;")
             self.btn_connect.setText("Conectar"); self.timer.stop()
        except Exception as e:
            self.connected = False; self.ser = None
            error_msg = f"Ocurrió un error inesperado ({type(e).__name__}) al conectar:\n{str(e)}"
            QtWidgets.QMessageBox.critical(self, "Error Inesperado", error_msg)
            self.connection_indicator.setStyleSheet("background-color: red; border-radius: 10px;")
            self.btn_connect.setText("Conectar"); self.timer.stop()

        self.update_status_label()

        for ch in range(8):
            self.channel_states[ch] = {'configured': False, 'tipo': None, 'gain': None, 'lp': None, 'hp': None}
            self.channel_cards[ch].setToolTip("Canal no configurado")
            self.channel_titles[ch].setToolTip("Canal no configurado")
            self.channel_cards[ch].setStyleSheet("QFrame { border: 1px solid #888; border-radius: 6px; }")
            self._reflow_plots_dynamic()

    def _disconnect_serial(self):

        self.timer.stop()
        if self.ser and self.ser.is_open:
            try:
                self.ser.close()
            except Exception as e:
                print(f"[ERROR] [RealTimePlot] Error al cerrar el puerto {self.serial_params.get('port', '')}: {e}")

        self.ser = None
        self.connected = False
        self.btn_connect.setText("Conectar")
        self.connection_indicator.setStyleSheet("background-color: red; border-radius: 10px;")
        self.update_status_label()

    def _smooth(self, y: np.ndarray, win: int) -> np.ndarray:
        """Media móvil con padding en los bordes (solo para visualización)."""
        if not isinstance(win, int) or win <= 1:
            return y
        if win % 2 == 0:  # que sea impar
            win += 1
        pad = win // 2
        # padding tipo 'edge' evita artefactos en los extremos
        ypad = np.pad(y, (pad, pad), mode='edge')
        kernel = np.ones(win, dtype=np.float64) / win
        return np.convolve(ypad, kernel, mode='valid')

    def _plot_channel(self, data, curve, plot):

        n = len(data)
        if n < 2:
            curve.clear()
            # rango mínimo para que ViewBox no compare con 0
            plot.setRange(xRange=(0.0, max(1.0 / max(self.sampling_rate, 1), 1e-3)), padding=0)
            return

        fs = float(self.sampling_rate)
        x = (np.arange(n, dtype=np.float64) / fs)
        y = np.asarray(data, dtype=np.float64)

        # --- Upsample (interpolación lineal solo para dibujar) ---
        k = int(getattr(self, "upsample_factor", 1))
        if k > 1:
            x_dense = np.linspace(x[0], x[-1], n * k, dtype=np.float64)
            y_dense = np.interp(x_dense, x, y).astype(np.float64)
        else:
            x_dense, y_dense = x, y

        # --- Suavizado opcional (media móvil) ---
        if getattr(self, "smooth_enabled", False):
            y_dense = self._smooth(y_dense, int(getattr(self, "smooth_window", 7)))

        # Dibujar
        curve.setData(x_dense, y_dense)

        # Rango X:  points_to_show y fs
        duration = max(len(data) / max(self.sampling_rate, 1.0), 1e-3)
        max_seconds = max(self.points_to_show / max(self.sampling_rate, 1.0), 1e-3)
        plot.setRange(xRange=(0.0, min(duration, max_seconds)), padding=0)


    def _get_channel_data_array(self, ch_idx: int) -> np.ndarray:
        # Devuelve el deque del canal como ndarray float64
        if ch_idx == 0: buf = self.dataA
        elif ch_idx == 1: buf = self.dataB
        elif ch_idx == 2: buf = self.dataC
        elif ch_idx == 3: buf = self.dataD
        elif ch_idx == 4: buf = self.dataE
        elif ch_idx == 5: buf = self.dataF
        elif ch_idx == 6: buf = self.dataG
        else:            buf = self.dataH
        if len(buf) < 8:
            return None
        return np.asarray(buf, dtype=np.float64)

    def _on_fft_open_clicked(self):
        ch_idx = self.fft_ch_combo.currentIndex()
        self._open_fft_for_channel(ch_idx)

    def _on_fft_close_clicked(self):
        ch_idx = self.fft_ch_combo.currentIndex()
        self._close_fft_for_channel(ch_idx)

    def _open_fft_for_channel(self, ch_idx: int):
        """Crea (o enfoca) una ventana FFT para 'ch_idx'. Persiste hasta cerrar."""
        if not (0 <= ch_idx < 8):
            return
        if ch_idx in self.fft_windows:
            # Ya existe: solo la trae al frente
            w = self.fft_windows[ch_idx]['win']
            w.show(); w.raise_(); w.activateWindow()
            return

        # Crear ventana y curva
        win = pg.plot(title=f"FFT Canal {ch_idx}")
        win.setLabel('bottom', 'Frecuencia (Hz)')
        win.setLabel('left', 'Magnitud')
        curve = win.plot(pen=pg.mkPen('c', width=2))
        self.fft_windows[ch_idx] = {'win': win, 'curve': curve}

        # Cuando la ventana se destruya, la sacamos del dict (persistencia controlada)
        def on_destroyed():
            self.fft_windows.pop(ch_idx, None)
        win.destroyed.connect(on_destroyed)


    def _compute_fft_bilateral(self, y: np.ndarray, fs: float):
        N = len(y)
        if N < 4 or fs <= 0:
            return None, None

        # Ventana Hann y zero-padding a potencia de 2
        win = np.hanning(N).astype(np.float64)
        yw = (y.astype(np.float64) * win)
        nfft = 1 << int(np.ceil(np.log2(N)))

        # FFT bilateral 
        Y = fft.fft(yw, n=nfft)
        f = fft.fftfreq(nfft, d=1.0/fs)
        Y_shift = fft.fftshift(Y)
        f_shift = fft.fftshift(f)

        # Magnitud normalizada
    
        mag = np.abs(Y_shift) / (np.sum(win) / 2.0 + 1e-12)
        return f_shift, mag

    def _refresh_all_ffts(self): #Refresca las ventanas FFT abiertas
        if not self.fft_windows:
            return

        fs = float(max(self.sampling_rate, 1.0))
        # Recorre canales abiertos
        for ch_idx, info in list(self.fft_windows.items()):
            # Solo si el canal está configurado y con datos
            if not self.channel_states[ch_idx]['configured']:
                info['curve'].clear()
                continue

            y = self._get_channel_data_array(ch_idx)
            if y is None:
                info['curve'].clear()
                continue

        
            y = y - np.mean(y)

            f, mag = self._compute_fft_bilateral(y, fs)
            mag = 20*np.log10(np.maximum(mag, 1e-12))
            info['curve'].setData(f, mag)
            info['win'].setLabel('left', 'Magnitud (dB)')


    def update_plot(self):
        if not self.connected or not self.ser or not self.ser.is_open:
            return

        try:
            # 1) Leer lo disponible (no bloquear) y acumular en buffer
            if self.ser.in_waiting > 0:
                self.buffer.extend(self.ser.read(self.ser.in_waiting))

            # 2) Decodificar algunos frames por tick (evita tirones)
            decoded = 0
            MAX_FRAMES_PER_TICK = 3  # ajusta 2..5 si quieres

            while True:
                # Mínimo: hdr(2) + nch(1) + nsamp(2) + seq(2) + chk(1) + al menos 2 bytes de datos
                if len(self.buffer) < 8:
                    break

                # Buscar cabecera A5 5A
                hdr_idx = self.buffer.find(self.FRAME_HDR)
                if hdr_idx == -1:
                    # no hay cabecera aún: descarta basura acumulada
                    self.buffer.clear()
                    break

                # Descartar bytes previos a la cabecera
                if hdr_idx > 0:
                    del self.buffer[:hdr_idx]

                # Ya tenemos al menos header + nch + nsamp + seq?
                if len(self.buffer) < 7:  # 2 (hdr) + 1 (nch) + 2 (nsamp) + 2 (seq)
                    break

                nch   = self.buffer[2]  # u8
                nsamp = int.from_bytes(self.buffer[3:5], 'little', signed=False)  # u16
                # seq = int.from_bytes(self.buffer[5:7], 'little', signed=False)   # opcional

                # Validación rápida de nch para evitar reshape raros
                if nch == 0 or nch > 8:
                    # valor imposible → resincroniza
                    del self.buffer[0]
                    continue

                data_bytes = nch * nsamp * 2
                total_len  = 2 + 1 + 2 + 2 + data_bytes + 1  # hdr + nch + nsamp + seq + data + chk

                # Esperar a que llegue el frame completo
                if len(self.buffer) < total_len:
                    break

                # Extraer frame
                frame = bytes(self.buffer[:total_len])

                # Checksum (suma de todo salvo el último byte)
                if (sum(frame[:-1]) & 0xFF) != frame[-1]:
                    # byte corrupto → avanza 1 y reintenta
                    del self.buffer[0]
                    continue

                # Payload: quitar hdr(2), nch(1), nsamp(2), seq(2) y chk(1)
                payload = frame[7:-1]

                # Convertir a matriz (nsamp, nch) de u16 LE
                try:
                    arr = np.frombuffer(payload, dtype='<u2').reshape(-1, nch)
                except ValueError:
                    # si por alguna razón no calza, resincroniza 1 byte
                    del self.buffer[0]
                    continue

                # 3) Convertir a voltios aplicando factor de escala
                scale = (self.v_ref / self.max_adc)

                # Siempre hay canal A
                chA = arr[:, 0].astype(np.float32) * scale
                # Conditional para B, C, D (si llegan)
                chB = arr[:, 1].astype(np.float32) * scale if nch >= 2 else None
                chC = arr[:, 2].astype(np.float32) * scale if nch >= 3 else None
                chD = arr[:, 3].astype(np.float32) * scale if nch >= 4 else None
                chE = arr[:, 6].astype(np.float32) * scale if nch >= 7 else None
                chF = arr[:, 5].astype(np.float32) * scale if nch >= 6 else None
                chG = arr[:, 4].astype(np.float32) * scale if nch >= 5 else None
                chH = arr[:, 7].astype(np.float32) * scale if nch >= 8 else None

                # 4) Acumular en buffers circulares
                if self.channel_states[0]['configured']: self.dataA.extend(chA.tolist())
                if chB is not None and self.channel_states[1]['configured']: self.dataB.extend(chB.tolist())
                if chC is not None and self.channel_states[2]['configured']: self.dataC.extend(chC.tolist())
                if chD is not None and self.channel_states[3]['configured']: self.dataD.extend(chD.tolist())
                if chE is not None and self.channel_states[4]['configured']: self.dataE.extend(chE.tolist())
                if chF is not None and self.channel_states[5]['configured']: self.dataF.extend(chF.tolist())
                if chG is not None and self.channel_states[6]['configured']: self.dataG.extend(chG.tolist())
                if chH is not None and self.channel_states[7]['configured']: self.dataH.extend(chH.tolist())

                # Consumir frame del buffer
                del self.buffer[:total_len]

                decoded += 1
                if decoded >= MAX_FRAMES_PER_TICK:
                    break

                # --- Pintado en orden: TOP (A,C,E,F) ; BOTTOM (B,D,G,H)
                if self.channel_states[0]['configured'] and len(self.dataA) > 0:
                    self._plot_channel(self.dataA, self.curve_chA, self.plot_chA)
                else:
                    self.curve_chA.clear()

                if self.channel_states[2]['configured'] and len(self.dataC) > 0:
                    self._plot_channel(self.dataC, self.curve_chC, self.plot_chC)
                else:
                    self.curve_chC.clear()

                if self.channel_states[4]['configured'] and len(self.dataE) > 0:
                    self._plot_channel(self.dataE, self.curve_chE, self.plot_chE)
                else:
                    self.curve_chE.clear()

                
                if self.channel_states[5]['configured'] and len(self.dataF) > 0:
                    self._plot_channel(self.dataF, self.curve_chF, self.plot_chF)
                else:
                    self.curve_chF.clear()


                if self.channel_states[1]['configured'] and len(self.dataB) > 0:
                    self._plot_channel(self.dataB, self.curve_chB, self.plot_chB)
                else:
                    self.curve_chB.clear()

                if self.channel_states[3]['configured'] and len(self.dataD) > 0:
                    self._plot_channel(self.dataD, self.curve_chD, self.plot_chD)
                else:
                    self.curve_chD.clear()

                if self.channel_states[6]['configured'] and len(self.dataG) > 0:
                    self._plot_channel(self.dataG, self.curve_chG, self.plot_chG)
                else:
                    self.curve_chG.clear()


                if self.channel_states[7]['configured'] and len(self.dataH) > 0:
                    self._plot_channel(self.dataH, self.curve_chH, self.plot_chH)
                else:
                    self.curve_chH.clear()

        except serial.SerialException as e:
            QtWidgets.QMessageBox.warning(self, "Error de Lectura",
                                        f"Se perdió la conexión o hubo un error:\n{str(e)}")
            self._disconnect_serial()
        except Exception as e:
            print("[ERROR] Excepción en update_plot:")
            traceback.print_exc()
            self.status_label.setText(f"Error: {type(e).__name__}")

    def _set_channel_state_card(self, ch_index: int, signal_type_idx: int, gain_idx: int, lp_idx: int, hp_idx: int):

        if not (0 <= ch_index < 8):
            return

        # Mapear índices a textos (ajusta a tus opciones reales si difieren)
        signal_types   = ["Nativa", "Rectificada", "Envolvente"]
        gains          = ["10", "25", "50"]
        lowpass_values = ["15 Hz", "20 Hz", "25 Hz"]
        highpass_values= ["150 Hz", "350 Hz", "400 Hz"]

        tipo = signal_types[signal_type_idx] if 0 <= signal_type_idx < len(signal_types) else str(signal_type_idx)
        gain = gains[gain_idx]               if 0 <= gain_idx        < len(gains)        else str(gain_idx)
        lp   = lowpass_values[lp_idx]        if 0 <= lp_idx          < len(lowpass_values) else str(lp_idx)
        hp   = highpass_values[hp_idx]       if 0 <= hp_idx          < len(highpass_values) else str(hp_idx)

        # Guardar estado
        st = self.channel_states[ch_index]
        st.update({'configured': True, 'tipo': tipo, 'gain': gain, 'lp': lp, 'hp': hp})

        # Tooltip en HTML (multilínea)
        tip_html = (
            f"<b>Canal:</b> {ch_index}<br>"
            f"<b>Tipo de Señal:</b> {tipo}<br>"
            f"<b>Ganancia:</b> {gain}<br>"
            f"<b>Filtro P. Bajos:</b> {lp}<br>"
            f"<b>Filtro P. Altos:</b> {hp}"
        )

        # Aplicar a la tarjeta y al título (para que el tooltip salga donde pases el mouse)
        self.channel_cards[ch_index].setToolTip(tip_html)
        self.channel_titles[ch_index].setToolTip(tip_html)

        # (Opcional) colorear borde si está configurado
        self.channel_cards[ch_index].setStyleSheet("QFrame { border: 1px solid #55aa55; border-radius: 6px; }")

    def _reflow_plots_dynamic(self):
        # 1) Limpiar la rejilla y ocultar
        while self.plots_grid.count():
            item = self.plots_grid.takeAt(0)
            w = item.widget()
            if w is not None:
                w.hide()

        configured = [i for i, st in enumerate(self.channel_states) if st.get('configured')]
        n = len(configured)
        if n == 0:
            # resetear stretches antiguos si los hubo
            for r in range(self._plots_rows_used):
                self.plots_grid.setRowStretch(r, 0)
            self._plots_rows_used = 0
            return

        # 2) Colocación regular: 2 por fila. El último (si n impar) se expande a dos columnas.
        rows = (n + 1) // 2  # filas necesarias
        for idx, ch in enumerate(configured):
            pw = self.plot_widgets[ch]
            pw.setSizePolicy(QtWidgets.QSizePolicy.Policy.Expanding,
                            QtWidgets.QSizePolicy.Policy.Expanding)

            # Si es el último y n es impar -> ocupa toda la fila
            if (idx == n - 1) and (n % 2 == 1):
                row = idx // 2
                self.plots_grid.addWidget(pw, row, 0, 1, 2)  # colspan=2
                pw.show()
            else:
                row = idx // 2
                col = idx % 2  # 0 izquierda, 1 derecha
                self.plots_grid.addWidget(pw, row, col)
                pw.show()

        # 3) Igualar ALTURA de todas las filas  (50/50, o 33/33/33, etc. según 'rows')
        #    Primero borra estiramientos anteriores que sobren.
        if self._plots_rows_used > rows:
            for r in range(rows, self._plots_rows_used):
                self.plots_grid.setRowStretch(r, 0)

        for r in range(rows):
            self.plots_grid.setRowStretch(r, 1)

        self._plots_rows_used = rows

    def _apply_plot_limits(self):
        max_seconds = max(self.points_to_show / max(self.sampling_rate, 1), 1e-3)
        for pw in [self.plot_chA, self.plot_chB, self.plot_chC, self.plot_chD,
                self.plot_chE, self.plot_chF, self.plot_chG, self.plot_chH]:
            pw.enableAutoRange(x=False, y=True)             # X manual, Y auto
            pw.setLimits(xMin=0.0, xMax=float(max_seconds)) # límites finitos
            pw.setRange(xRange=(0.0, float(max_seconds)), padding=0)


    def _clear_channel_buffer(self, ch: int):
        if ch == 0:  self.dataA.clear(); self.curve_chA.clear()
        elif ch == 1: self.dataB.clear(); self.curve_chB.clear()
        elif ch == 2: self.dataC.clear(); self.curve_chC.clear()
        elif ch == 3: self.dataD.clear(); self.curve_chD.clear()
        elif ch == 4: self.dataE.clear(); self.curve_chE.clear()
        elif ch == 5: self.dataF.clear(); self.curve_chF.clear()
        elif ch == 6: self.dataG.clear(); self.curve_chG.clear()
        elif ch == 7: self.dataH.clear(); self.curve_chH.clear()


    def closeEvent(self, event: QtGui.QCloseEvent):
        self._disconnect_serial()
        event.accept()


    def _update_channel_labels(self):

        return


    def _show_channel_config_dialog(self):
        
        if not self.connected:
            QtWidgets.QMessageBox.warning(self, "Error", "Conéctese al puerto serial primero.")
            return

        dialog = ChannelConfigDialog(self.channel_params, self)
        if dialog.exec() == QtWidgets.QDialog.DialogCode.Accepted:
            new_config = dialog.get_config()
            self.channel_params.update(new_config)
            self._update_channel_labels()
            
            # Enviar al STM
            self._send_command_to_stm()

            # Actualizar la cajita del canal en "Configuración Actual"
            self._set_channel_state_card(
                self.channel_params["channel"],       # canal (0..7)
                self.channel_params["signal_type"],   # tipo de señal (índice)
                self.channel_params["gain"],          # ganancia (índice)
                self.channel_params["lowpass"],       # filtro LP (índice)
                self.channel_params["highpass"]       # filtro HP (índice)
            )

    def _send_command_to_stm(self):
        if not self.connected or not self.ser or not self.ser.is_open:
             QtWidgets.QMessageBox.warning(self,"Error","No hay conexión serial activa para enviar comando.")
             return
        try:
            cmd = (
                f"{self.channel_params['channel']}"
                f"{self.channel_params['gain']}"
                f"{self.channel_params['lowpass']}"
                f"{self.channel_params['highpass']}"
                f"{self.channel_params['signal_type']}"
            )
            self.ser.write(cmd.encode('utf-8'))
            print(f"[DEBUG] Comando enviado: {cmd}")

            self._set_channel_state_card(
            self.channel_params["channel"],
            self.channel_params["signal_type"],
            self.channel_params["gain"],
            self.channel_params["lowpass"],
            self.channel_params["highpass"],
        )

            ch = self.channel_params["channel"]
            self.channel_states[ch]['configured'] = True
            self._clear_channel_buffer(ch)
            self._reflow_plots_dynamic() 

        except serial.SerialException as e:
             QtWidgets.QMessageBox.critical(self,"Error de Envío",f"Error serial al enviar comando:\n{str(e)}")

        except Exception as e:
            QtWidgets.QMessageBox.critical(
                self,
                "Error de Envío",
                f"Fallo al enviar comando al STM32 ({type(e).__name__}):\n{str(e)}"
            )

# --- Punto de Entrada Principal ---
if __name__ == '__main__':
    app = QtWidgets.QApplication(sys.argv)
    window = RealTimePlot()
    window.resize(1200, 700)
    window.show()
    sys.exit(app.exec())