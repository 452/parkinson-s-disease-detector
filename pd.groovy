@Grab(group='org.knowm.xchart', module='xchart', version='3.2.2')
@Grab(group='org.apache.commons', module='commons-collections4', version='4.1')
@Grab(group='org.apache.camel', module='camel-websocket', version='2.18.1')
@Grab(group='org.apache.camel', module='camel-ahc-ws', version='2.18.1')
@Grab(group='org.apache.camel', module='camel-bindy', version='2.18.1')
@Grab(group='org.apache.camel', module='camel-metrics', version='2.18.1')
@Grab('org.apache.camel:camel-core:2.18.1')
@Grab('org.slf4j:slf4j-simple:1.7.22')

import org.apache.camel.*
import org.apache.camel.impl.*
import org.apache.camel.builder.*
import org.apache.camel.dataformat.bindy.annotation.CsvRecord
import org.apache.camel.dataformat.bindy.annotation.DataField
import org.apache.camel.dataformat.bindy.csv.BindyCsvDataFormat
import org.apache.camel.util.toolbox.*
import org.apache.camel.processor.*
import org.apache.camel.processor.aggregate.AbstractListAggregationStrategy
import java.nio.ByteBuffer
import java.nio.ByteOrder

import org.knowm.xchart.*;
import org.knowm.xchart.style.markers.*
import java.util.Queue;
import org.apache.commons.collections4.queue.CircularFifoQueue;

String uri = "ahc-ws://192.168.220.165:81";

def camelContext = new DefaultCamelContext()
// camelContext.setTracing(true);
BindyCsvDataFormat packetDataFormat = new BindyCsvDataFormat(Packet.class);

Queue<Packet> fifo = new CircularFifoQueue<Packet>(128);

def chartsTypes = ['Accelerometer', 'Gyroscope', 'Magnetometer', 'Yaw,Pitch,Roll', 'Themperature']
 
List<XYChart> charts = new ArrayList<XYChart>();
 
chartsTypes.each { type ->
  XYChart chart = new XYChartBuilder().title(type).xAxisTitle("X").yAxisTitle("Y").width(600).height(400).build();
  if (type == 'Themperature') {
    XYSeries series = chart.addSeries('T', null, [0]);
    series.setMarker(SeriesMarkers.NONE);
  } else {
    (0..2).each {
      XYSeries series = chart.addSeries("$it", null, [0]);
      series.setMarker(SeriesMarkers.NONE);
    }
  }
  charts.add(chart);
}

def sw = new SwingWrapper<XYChart>(charts).displayChartMatrix();

camelContext.addRoutes(new RouteBuilder() {
    def void configure() {
		from(uri)
		.routeId("ws received")
		.process(new Processor() {
			def void process(Exchange exchange) {
				Packet p = new Packet(exchange.getIn().getBody(byte[].class))
				exchange.getOut().setBody(p)
                // println p
                fifo.add(p);
                def data = [[],[],[],[],[],[],[],[],[],[],[],[],[],[]];
                fifo.eachWithIndex { v, index ->
                    data[0] << v.uptime
                    data[1] << v.accelerometerX
                    data[2] << v.accelerometerY
                    data[3] << v.accelerometerZ
                    data[4] << v.gyroscopeX
                    data[5] << v.gyroscopeY
                    data[6] << v.gyroscopeZ
                    data[7] << v.magnetometerX
                    data[8] << v.magnetometerY
                    data[9] << v.magnetometerZ
                    data[10] << v.yaw
                    data[11] << v.pitch
                    data[12] << v.roll
                    data[13] << v.temperature
                }
                charts[0].updateXYSeries('0', data[0], data[1], null);
                charts[0].updateXYSeries('1', data[0], data[2], null);
                charts[0].updateXYSeries('2', data[0], data[3], null);
                charts[1].updateXYSeries('0', data[0], data[4], null);
                charts[1].updateXYSeries('1', data[0], data[5], null);
                charts[1].updateXYSeries('2', data[0], data[6], null);
                charts[2].updateXYSeries('0', data[0], data[7], null);
                charts[2].updateXYSeries('1', data[0], data[8], null);
                charts[2].updateXYSeries('2', data[0], data[9], null);
                charts[3].updateXYSeries('0', data[0], data[10], null);
                charts[3].updateXYSeries('1', data[0], data[11], null);
                charts[3].updateXYSeries('2', data[0], data[12], null);
                charts[4].updateXYSeries("T", data[0], data[13], null);
                sw.repaint();
			}
		})
		.aggregate(constant(true), new ListOfPacketsStrategy())
        .completionSize(300)
		.marshal(packetDataFormat)
		.to('file://?fileName=pd.csv&fileExist=Append')
		.log('added 300 samples')
    }
})
camelContext.start()

addShutdownHook{ camelContext.stop() }
synchronized(this){
	this.wait()
}

@CsvRecord(separator=",", generateHeaderColumns=false)
class Packet {

	def final byte code;
    @DataField(pos = 1, precision = 6)
    def final float temperature;
    @DataField(pos = 2, precision = 8)
    def final float gyroscopeX;
    @DataField(pos = 3, precision = 8)
    def final float gyroscopeY;
    @DataField(pos = 4, precision = 8)
    def final float gyroscopeZ;
    @DataField(pos = 5, precision = 8)
    def final float accelerometerX;
    @DataField(pos = 6, precision = 8)
    def final float accelerometerY;
    @DataField(pos = 7, precision = 8)
    def final float accelerometerZ;
    @DataField(pos = 8, precision = 8)
    def final float magnetometerX;
    @DataField(pos = 9, precision = 8)
    def final float magnetometerY;
    @DataField(pos = 10, precision = 8)
    def final float magnetometerZ;
    @DataField(pos = 11, precision = 8)
    def final float yaw;
    @DataField(pos = 12, precision = 8)
    def final float pitch;
    @DataField(pos = 13, precision = 8)
    def final float roll;
    @DataField(pos = 14, precision = 8)
    def final int vcc;
    @DataField(pos = 15)
    def final int uptime;
    @DataField(pos = 16)
    def final long time;
    @DataField(pos = 17)
    def final float hz;

    public Packet(byte[] bytes) {
    	try {
			ByteBuffer bb = ByteBuffer.wrap(bytes);
			bb.order(ByteOrder.LITTLE_ENDIAN);
			code = bb.getInt()
			if (code == 9) {
                accelerometerX = bb.getFloat();
                accelerometerY = bb.getFloat();
                accelerometerZ = bb.getFloat();
				gyroscopeX = bb.getFloat();
				gyroscopeY = bb.getFloat();
				gyroscopeZ = bb.getFloat();
				magnetometerX = bb.getFloat();
				magnetometerY = bb.getFloat();
				magnetometerZ = bb.getFloat();
                yaw = bb.getFloat();
                pitch = bb.getFloat();
                roll = bb.getFloat();
				vcc = bb.getFloat();
				uptime = bb.getInt();
				time = bb.getInt();
				temperature = bb.getFloat();
                hz = bb.getFloat();
                // accelerometerX = (int) 1000 * accelerometerX;
                // accelerometerY = (int) 1000 * accelerometerY;
                // accelerometerZ = (int) 1000 * accelerometerZ;
                // gyroscopeX = (int) gyroscopeX;
                // gyroscopeY = (int) gyroscopeY;
                // gyroscopeZ = (int) gyroscopeZ;
                // magnetometerX = (int) magnetometerX;
                // magnetometerY = (int) magnetometerY;
                // magnetometerZ = (int) magnetometerZ;
                vcc = vcc / 1000.0;
                temperature = temperature / 333.87 + 21.0;
			}
    	} catch (Exception e) {
    		println 'please check your data' + e
    	}
    }

    String toString() {
    	"""code: $code uptime: $uptime time: ${new Date(time*1000)} vcc: $vcc temperature: $temperature
    	gyroscopeX: $gyroscopeX gyroscopeY: $gyroscopeY gyroscopeZ: $gyroscopeZ
    	accelerometerX: $accelerometerX accelerometerY: $accelerometerY accelerometerZ: $accelerometerZ
    	magnetometerX: $magnetometerX magnetometerY: $magnetometerY magnetometerZ: $magnetometerZ
        yaw: $yaw pitch: $pitch roll: $roll
        hz: $hz"""
    }
}

public final class ListOfPacketsStrategy extends AbstractListAggregationStrategy<Packet> {
	@Override
	public Packet getValue(Exchange exchange) {
		return exchange.getIn().getBody(Packet.class);
	}
}