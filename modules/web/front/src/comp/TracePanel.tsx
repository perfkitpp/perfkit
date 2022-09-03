import {theme} from "../App";
import {useWebSocket} from "../Utils";

export default function TracePanel(prop: { socketUrl: string }) {
  const sockTrace = useWebSocket(prop.socketUrl, {

  }, []);

  return <div>Trace Panel</div>;
}