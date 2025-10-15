import logging
import pendulum
import pandas as pd

from airflow.decorators import dag, task
from airflow.providers.influxdb.hooks.influxdb import InfluxDBHook

TABLE = 'sensors'
BUCKET = 'iot-ex2'
TIME_RANGE = '1m'
ALLOWED_RANGE = (15, 35)

@dag(
    schedule="*/1 * * * *",
    start_date=pendulum.datetime(2025, 1, 1, tz="UTC"),
    catchup=False,
    tags=["hotpath", "influxdb2", "temperature"],
)
def temperature_hotpath():

    @task()
    def get_sensors_data():
        influx_hook = InfluxDBHook(conn_id="influxdb")

        flux_query = '''
        from(bucket: "{BUCKET}")
        |> range(start: -{TIME_RANGE})
        |> filter(fn: (r) => r._measurement == "{TABLE}")
        |> filter(fn: (r) => r._field == "t")
        |> sort(columns: ["_time"])
        '''

        df = influx_hook.query_to_df(flux_query)

        # Convert Timestamp columns to strings to make XCom-safe
        for col in df.columns:
            if pd.api.types.is_datetime64_any_dtype(df[col]):
                df[col] = df[col].astype(str)
                
        if "_value" not in df.columns or "_time" not in df.columns:
            raise ValueError("Expected '_value' and '_time' in query result")

        return df[["_value", "_time", "sensorID"]].rename(columns={"_value": "t"}).to_dict(orient="records")

    @task()
    def check_temperature_threshold(rows: list[dict]):
        for row in rows:
            temp = row.get("t")
            ts = row.get("_time")
            sensor_id = row.get("sensorID", "unknown")

            if temp is None:
                logging.warning(f"Missing temperature value for sensor {sensor_id} at {ts}")
                continue

            if not (ALLOWED_RANGE[0] <= temp <= ALLOWED_RANGE[1]):
                logging.warning(f"sensor {sensor_id} = {temp}°C at {ts} (out of range)")
            else:
                logging.info(f"sensor {sensor_id} = {temp}°C at {ts} (within range)")

    data = get_sensors_data()
    check_temperature_threshold(data)

temperature_hotpath()
