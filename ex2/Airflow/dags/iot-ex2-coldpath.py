import logging
import numpy as np
import pandas as pd
import pendulum

from airflow.decorators import dag, task
from airflow.providers.influxdb.hooks.influxdb import InfluxDBHook

TABLE = 'sensors'
BUCKET = 'iot-ex2'
TIME_RANGE = '1h'
SENSOR_ID_1 = '1'
SENSOR_ID_2 = '2'

@dag(
    schedule="0 * * * *",
    start_date=pendulum.datetime(2025, 1, 1, tz="UTC"),
    catchup=False,
    tags=["coldpath", "influxdb2", "taskflow"],
)
def temperature_cold_path():

    @task()
    def get_sensor_data(sensor_id: str) -> list[dict]:
        import pandas as pd
        influx_hook = InfluxDBHook(conn_id="influxdb")

        flux_query = f'''
        from(bucket: "{BUCKET}")
        |> range(start: -{TIME_RANGE})
        |> filter(fn: (r) => r._measurement == "{TABLE}")
        |> filter(fn: (r) => r["sensorID"] == "{sensor_id}")
        |> filter(fn: (r) => r._field == "t")
        |> sort(columns: ["_time"])
        '''

        df = influx_hook.query_to_df(flux_query)

        # Ensure datetime columns are XCom-safe
        for col in df.columns:
            if pd.api.types.is_datetime64_any_dtype(df[col]):
                df[col] = df[col].astype(str)

        # Keep only necessary columns for comparison
        if "_value" not in df.columns or "_time" not in df.columns:
            raise ValueError("Expected '_value' and '_time' in query result")

        return df[["_value", "_time"]].rename(columns={"_value": "t"}).to_dict(orient="records")


    @task()
    def compare_and_detect(sensor_a_data: list[dict], sensor_b_data: list[dict]) -> None:
        values_a = [row["t"] for row in sensor_a_data]
        values_b = [row["t"] for row in sensor_b_data]

        if not values_a or not values_b:
            logging.warning("One or both sensor data lists are empty.")
            return

        mean_a = np.mean(values_a)
        mean_b = np.mean(values_b)

        p95_a = np.percentile(values_a, 95)
        p95_b = np.percentile(values_b, 95)

        diff_means = abs(mean_a - mean_b)
        diff_p95s = abs(p95_a - p95_b)

        logging.info(f"Mean A: {mean_a:.2f}, Mean B: {mean_b:.2f}, Diff: {diff_means:.2f}")
        logging.info(f"P95 A: {p95_a:.2f}, P95 B: {p95_b:.2f}, Diff: {diff_p95s:.2f}")

        if diff_means > diff_p95s:
            logging.warning(f"Difference in means ({diff_means:.2f}) exceeds difference in P95s ({diff_p95s:.2f})")
        else:
            logging.info(f"Difference in means ({diff_means:.2f}) is within expected variation ({diff_p95s:.2f})")


    data_a = get_sensor_data(SENSOR_ID_1)
    data_b = get_sensor_data(SENSOR_ID_2)
    compare_and_detect(data_a, data_b)

temperature_cold_path()
