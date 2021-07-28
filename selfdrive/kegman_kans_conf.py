import json
import os

class kegman_kans_conf():
  def __init__(self, CP=None):
    self.conf = self.read_config()
    if CP is not None:
      self.init_config(CP)

  def init_config(self, CP):
    write_conf = False
    if self.conf['tuneGernby'] != "1":
      self.conf['tuneGernby'] = str(1)
      write_conf = True

    # only fetch Kp, Ki, Kf sR and sRC from interface.py if it's a PID controlled car
    if CP.lateralTuning.which() == 'pid':
      if self.conf['Kp'] == "-1":
        self.conf['Kp'] = str(round(CP.lateralTuning.pid.kpV[0],3))
        write_conf = True
      if self.conf['Ki'] == "-1":
        self.conf['Ki'] = str(round(CP.lateralTuning.pid.kiV[0],3))
        write_conf = True
      if self.conf['Kd'] == "-1":
        self.conf['Kd'] = str(round(CP.lateralTuning.pid.kdV[0],3))
        write_conf = True
      if self.conf['Kf'] == "-1":
        self.conf['Kf'] = str('{:f}'.format(CP.lateralTuning.pid.kf))
        write_conf = True

    if write_conf:
      self.write_config(self.config)

  def read_config(self):
    self.element_updated = False

    if os.path.isfile('/data/kegman_kans.json'):
      with open('/data/kegman_kans.json', 'r') as f:
        self.config = json.load(f)

      if "battPercOff" not in self.config:
        self.config.update({"battPercOff":"80"})
        self.config.update({"carVoltageMinEonShutdown":"12000"})
        self.config.update({"brakeStoppingTarget":"0.65"})
        self.element_updated = True

      if "tuneGernby" not in self.config:
        self.config.update({"tuneGernby":"1"})
        self.config.update({"Kp":"0.192"})
        self.config.update({"Ki":"0.021"})
        self.config.update({"Kd":"0.00032"})
        self.element_updated = True

      if "liveParams" not in self.config:
        self.config.update({"liveParams":"1"})
        self.element_updated = True

      if "useLiveSteerRatio" not in self.config:
        self.config.update({"useLiveSteerRatio":"1"})
        self.element_updated = True

      if "steerRatio" not in self.config:
        self.config.update({"steerRatio":""})
        self.config.update({"steerRateCost":""})
        self.config.update({"steerActuatorDelay":""})
        self.element_updated = True

      if "ONE_BAR_DISTANCE" not in self.config:
        self.config.update({"ONE_BAR_DISTANCE":"1.0"})
        self.config.update({"TWO_BAR_DISTANCE":"1.8"})
        self.config.update({"THREE_BAR_DISTANCE":"3.6"})
        self.config.update({"STOPPING_DISTANCE":"1.5"})
        self.element_updated = True

      if "deadzone" not in self.config:
        self.config.update({"deadzone":"0"})
        self.element_updated = True

      if "1barBP0" not in self.config:
        self.config.update({"1barBP0":"-0.3"})
        self.config.update({"1barBP1":"1.8"})
        self.config.update({"2barBP0":"-0.2"})
        self.config.update({"2barBP1":"2.25"})
        self.config.update({"3barBP0":"-0.1"})
        self.config.update({"3barBP1":"4.05"})
        self.element_updated = True

      if "1barMax" not in self.config:
        self.config.update({"1barMax":"1.3"})
        self.config.update({"2barMax":"2.0"})
        self.config.update({"3barMax":"3.85"})
        self.element_updated = True

      if "1barHwy" not in self.config:
        self.config.update({"1barHwy":"0.4"})
        self.config.update({"2barHwy":"0.3"})
        self.config.update({"3barHwy":"0.2"})
        self.element_updated = True

      if "slowOnCurves" not in self.config:
        self.config.update({"slowOnCurves":"1"})
        self.element_updated = True

      if "Kf" not in self.config:
        self.config.update({"Kf":"0.00007"})
        self.element_updated = True

      if "sR_boost" not in self.config:
        self.config.update({"sR_boost":""})
        self.config.update({"sR_BP0":""})
        self.config.update({"sR_BP1":""})
        self.config.update({"sR_time":""})
        self.element_updated = True

      if "ALCnudgeLess" not in self.config:
        self.config.update({"ALCnudgeLess":""})
        self.config.update({"ALCminSpeed":""})
        self.element_updated = True

      if "ALCtimer" not in self.config:
        self.config.update({"ALCtimer":""})
        self.element_updated = True
# AutoHold
      if "AutoHold" not in self.config:
        self.config.update({"AutoHold":"1"})
        self.element_updated = True

      if "nTune" not in self.config:
        self.config.update({"nTune":"1"})
        self.element_updated = True

      if "steerLimitTimer" not in self.config:
        self.config.update({"steerLimitTimer":"0.7"})
        self.element_updated = True

      if "CruiseDelta" not in self.config:
        self.config.update({"CruiseDelta":"5"})
        self.element_updated = True

      if "CruiseEnableMin" not in self.config:
        self.config.update({"CruiseEnableMin":"10"})
        self.element_updated = True

      if "epsModded" not in self.config:
        self.config.update({"epsModded":"0"})
        self.element_updated = True

      if "accelerationMode" not in self.config:
        self.config.update({"accelerationMode":"0"})
        self.element_updated = True

      if self.element_updated:
        print("updated")
        self.write_config(self.config)

    else:  # add "accelerationmode":"0" like as bellow, 1st word on the 2nd line
      self.config = {"lastTrMode":"2", "battChargeMin":"60", "battChargeMax":"75", "wheelTouchSeconds":"18000", \
                     "accelerationMode":"0", "battPercOff":"80", "carVoltageMinEonShutdown":"12000", \
                     "brakeStoppingTarget":"0.65", "tuneGernby":"1", "AutoHold":"1", "steerLimitTimer":"0.7", \
                     "Kp":"0.192", "Ki":"0.021", "Kd":"0.00032", "Kf":"0.00007", "liveParams":"1", "deadzone":"0.0", \
                     "1barBP0":"-0.3", "2barBP0":"-0.2", "3barBP0":"-0.1", \
                     "1barBP1":"1.8", "2barBP1":"2.25", "3barBP1":"4.05", \
                     "steerRatio":"15.07", "steerRateCost":"0.66", "steerActuatorDelay":"0.075", \
                     "1barMax":"1.3", "2barMax":"2.05", "3barMax":"3.85", "STOPPING_DISTANCE":"1.5", \
                     "ONE_BAR_DISTANCE":"1.0", "TWO_BAR_DISTANCE":"1.8", "THREE_BAR_DISTANCE":"3.6", \
                     "1barHwy":"0.4", "2barHwy":"0.3", "3barHwy":"0.2", "nTune":"1", "useLiveSteerRatio":"1", \
                     "sR_boost":"5.0", "sR_BP0":"1.44", "sR_BP1":"25", "sR_time":"7.0", \
                     "ALCnudgeLess":"1", "ALCminSpeed":"8.6", "ALCtimer":"0.5", "CruiseDelta":"5", \
                     "CruiseEnableMin":"10", "epsModded": "0", "slowOnCurves":"1"}


      self.write_config(self.config)
    return self.config

  def write_config(self, config):
    try:
      with open('/data/kegman_kans.json', 'w') as f:
        json.dump(self.config, f, indent=2, sort_keys=True)
        os.chmod("/data/kegman_kans.json", 0o764)
    except IOError:
      os.mkdir('/data')
      with open('/data/kegman_kans.json', 'w') as f:
        json.dump(self.config, f, indent=2, sort_keys=True)
        os.chmod("/data/kegman_kans.json", 0o764)