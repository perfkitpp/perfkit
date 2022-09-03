import {createContext, CSSProperties, useEffect, useState} from 'react';
import './App.scss';
import Terminal from "./comp/Terminal";
import {Button, Container, Row, Col, ColProps} from "react-bootstrap";
import ConfigPanel from "./comp/ConfigPanel";
import TracePanel from './comp/TracePanel'

export const socketUrlPrefix = process.env.NODE_ENV === "development"
  ? `ws://${window.location.hostname}:15572`
  : "ws://" + window.location.host

interface ToggleRibbonProps {
  enableState: boolean
  setEnableState: (v: boolean | ((v: boolean) => boolean)) => void;
  iconClass: string;
  labelText: string;
  toolTip: string;
}

const curStyle = getComputedStyle(document.body);
export const theme = {
  primary: curStyle.getPropertyValue('--bs-primary'),
  secondary: curStyle.getPropertyValue('--bs-secondary'),
  success: curStyle.getPropertyValue('--bs-success'),
  info: curStyle.getPropertyValue('--bs-info'),
  warning: curStyle.getPropertyValue('--bs-warning'),
  danger: curStyle.getPropertyValue('--bs-danger'),
  light: curStyle.getPropertyValue('--bs-light'),
  dark: curStyle.getPropertyValue('--bs-dark'),
}

function ToggleRibbon(prop: ToggleRibbonProps) {
  const [mouseHover, setMouseHover] = useState(false);

  useEffect(() => {
  }, [prop.enableState]);

  return <div className='w-100'>
    <Button
      title={prop.toolTip}
      className='d-flex flex-row w-100 align-items-center'
      variant={prop.enableState ? 'primary' : 'outline-secondary'}
      onClick={() => prop.setEnableState(v => !v)}
      onMouseEnter={() => setMouseHover(true)}
      onMouseLeave={() => setMouseHover(false)}>
      <i className={prop.iconClass + ' ToggleRibbon-icon flex-grow-1'}>{mouseHover &&
          <span className='ps-2'>{prop.labelText}</span>}
      </i>
    </Button>
  </div>;
}

interface ModulePanelProps {
  title?: string;
  iconClass?: string;
  colAttr?: any;
  styleVars?: CSSProperties;

  children?: React.ReactNode;
}

export function ModulePanelCol(prop: ModulePanelProps) {
  return (
    <Col {...prop.colAttr}
         className='mb-2 ps-3 pe-0 me-2'
         style={{
           // display: prop.enabled ? 'block' : 'none',
           overflowY: 'hidden',
           flexDirection: 'column',
           maxHeight: '100%',
           ...prop.styleVars
         }}>
      <div className='ModulePanel d-flex flex-column'>
        <div className='text-center flex-grow-0 align-items-center'>
          <i className={`${prop.iconClass} fw-bold w-100`}
             style={{fontSize: '1.2em'}}/>
          <span className='ms-1 fw-bold' style={{fontSize: '1.1em'}}>
          {prop.title}
          </span>
        </div>
        <div style={{flex: '1', height: "100%", overflowY: 'auto'}}>
          {prop.children}
        </div>
      </div>
    </Col>
  );
}

interface EnableStatus {
  terminal: boolean;
  system: boolean;
  graphics: boolean;
  configs: boolean;
  traces: boolean;
  plottings: boolean;
}

const initStateCfgStr = localStorage.getItem('PerfkitWeb/EnableStatus');
const initStateCfg = JSON.parse(initStateCfgStr || "{}") as EnableStatus;

function App() {
  const [enableTerminal, setEnableTerminal] = useState((initStateCfg.terminal ??= true) as boolean);
  const [enableSystemInfo, setEnableSystemInfo] = useState((initStateCfg.system ??= true) as boolean);
  const [enableGraphics, setEnableGraphics] = useState((initStateCfg.graphics ??= false) as boolean);
  const [enableConfigs, setEnableConfigs] = useState((initStateCfg.configs ??= true) as boolean);
  const [enableTraces, setEnableTraces] = useState((initStateCfg.traces ??= true) as boolean);
  const [enablePlottings, setEnablePlottings] = useState((initStateCfg.plottings ??= false) as boolean);

  useEffect(() => {
    let obj = {} as EnableStatus;
    obj.terminal = enableTerminal;
    obj.system = enableSystemInfo;
    obj.graphics = enableGraphics;
    obj.configs = enableConfigs;
    obj.traces = enableTraces;
    obj.plottings = enablePlottings;
    localStorage.setItem('PerfkitWeb/EnableStatus', JSON.stringify(obj));
  }, [enableTerminal, enableSystemInfo, enableGraphics, enableConfigs, enableTraces, enablePlottings]);

  return (
    <div className='App d-flex flex-column'>
      <div className='border-bottom border-2 border-primary bg-primary bg-opacity-10 py-2 px-3 flex-grow-0'>
        <Row className=''>
          <Col>
            <ToggleRibbon enableState={enableSystemInfo}
                          setEnableState={setEnableSystemInfo}
                          iconClass={'ri-checkbox-multiple-blank-fill'}
                          labelText={'System'}
                          toolTip={'Show/Hide system stat panel'}/>
          </Col>
          <Col>
            <ToggleRibbon enableState={enableTerminal}
                          setEnableState={setEnableTerminal}
                          iconClass={'ri-terminal-window-fill'}
                          labelText={'Terminal'}
                          toolTip={'Show/Hide terminal'}/>
          </Col>
          <Col>
            <ToggleRibbon enableState={enableGraphics}
                          setEnableState={setEnableGraphics}
                          iconClass={'ri-artboard-line'}
                          labelText={'Graphics'}
                          toolTip={'Show/Hide grahpics panel'}/>
          </Col>
          <Col>
            <ToggleRibbon enableState={enableConfigs}
                          setEnableState={setEnableConfigs}
                          iconClass={'ri-list-settings-fill'}
                          labelText={'Configs'}
                          toolTip={'Show/Hide terminal'}/>
          </Col>
          <Col>
            <ToggleRibbon enableState={enableTraces}
                          setEnableState={setEnableTraces}
                          iconClass={'ri-dashboard-2-line'}
                          labelText={'Traces'}
                          toolTip={'Show/Hide trace panel'}/>
          </Col>
          <Col>
            <ToggleRibbon enableState={enablePlottings}
                          setEnableState={setEnablePlottings}
                          iconClass={'ri-line-chart-line'}
                          labelText={'Plots'}
                          toolTip={'Show/Hide plots panel'}/>
          </Col>
        </Row>
      </div>
      <Container fluid className='mt-0 overflow-auto flex-grow-1'>
        <Row className='py-1 h-100'>
          {enableSystemInfo &&
              <ModulePanelCol colAttr={{sm: true}} styleVars={{minWidth: '60ch'}}
                              title='System Status' iconClass='ri-checkbox-multiple-blank-fill'>
                  System Info window will be placed here.
              </ModulePanelCol>}
          {enableTerminal &&
              <ModulePanelCol colAttr={{xxl: true}} styleVars={{minWidth: '140ch'}}
                              title='Terminal' iconClass='ri-terminal-line'>
                  <Terminal socketUrl={socketUrlPrefix + '/ws/tty'}/>
              </ModulePanelCol>}
          {enableGraphics &&
              <ModulePanelCol styleVars={{minWidth: '100ch'}}
                              title='Grahpics' iconClass='ri-artboard-line'>
                  Graphics window will be placed here.
              </ModulePanelCol>}
          {enableConfigs &&
              <ModulePanelCol colAttr={{lg: true}} styleVars={{minWidth: '80ch'}}
                              title='Configs' iconClass='ri-list-settings-fill'>
                  <ConfigPanel socketUrl={socketUrlPrefix + '/ws/config'}/>
              </ModulePanelCol>}
          {enableTraces &&
              <ModulePanelCol colAttr={{md: true}} styleVars={{minWidth: '80ch'}}
                              title='Traces' iconClass='ri-dashboard-2-line'>
                  <TracePanel socketUrl={socketUrlPrefix + '/ws/trace'}/>
              </ModulePanelCol>}
          {enablePlottings &&
              <ModulePanelCol styleVars={{minWidth: '80ch'}}
                              title='Plotting' iconClass='ri-line-chart-line'>
                  Plotting window will be placed here.
              </ModulePanelCol>}
        </Row>
      </Container>
    </div>
  );
}

export default App;
