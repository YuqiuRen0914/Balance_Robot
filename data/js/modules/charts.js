// /assets/js/modules/charts.js
import { state, domElements, CONSTANTS } from '../config.js';
import { appendLog } from '../ui.js';

/**
 * 创建一个图表实例
 * @param {HTMLCanvasElement} canvas 
 * @param {string[]} labels 
 * @returns {Chart|null}
 */
function createChart(canvas, labels) {
  if (!window.Chart) {
    canvas.parentElement.innerHTML = '<div class="readout">Chart.js 未加载，图表不可用</div>';
    return null;
  }
  return new Chart(canvas, {
    type: 'line',
    data: {
      labels: Array.from({ length: CONSTANTS.MAX_CHART_POINTS }, (_, i) => i),
      datasets: labels.map(label => ({
        label,
        data: Array(CONSTANTS.MAX_CHART_POINTS).fill(0),
        borderWidth: 1.5,
        tension: 0.2,
        pointRadius: 0,
      }))
    },
    options: {
      animation: false,
      responsive: true,
      maintainAspectRatio: false,
      scales: {
        x: { display: false },
        y: { 
          beginAtZero: false, 
          ticks: { 
            maxTicksLimit: 5,
            callback: function(value) {
              return value.toFixed(3);
            }
          }
        }
      },
      plugins: {
        legend: {
          display: true,
          position: 'bottom',
          labels: { boxWidth: 12, padding: 10 },
        }
      },
      elements: {
        line: {
          borderJoinStyle: 'round'
        }
      }
    }
  });
}

/**
 * 初始化所有图表
 */
export function initCharts() {
  state.charts.chart1 = createChart(domElements.chart1.canvas, ['a', 'b', 'c']);
  state.charts.chart2 = createChart(domElements.chart2.canvas, ['d', 'e', 'f']);
  state.charts.chart3 = createChart(domElements.chart3.canvas, ['g', 'h', 'i']);
  appendLog('[INIT] charts ready');
}

/**
 * 应用从后端接收的图表配置
 * @param {Array<object>} config 
 */
export function applyChartConfig(config) {
  if (!Array.isArray(config)) return;

  const chartRefs = [
    { chart: state.charts.chart1, titleEl: domElements.chart1.title },
    { chart: state.charts.chart2, titleEl: domElements.chart2.title },
    { chart: state.charts.chart3, titleEl: domElements.chart3.title },
  ];

  config.forEach((c, i) => {
    const { chart, titleEl } = chartRefs[i];
    if (titleEl && c.title) titleEl.textContent = c.title;

    if (chart && Array.isArray(c.legends)) {
      c.legends.forEach((name, j) => {
        if (chart.data.datasets[j]) {
          chart.data.datasets[j].label = name;
        }
      });
      chart.update('none');
    }
  });
}

/**
 * 向图表推送一组新数据
 * @param {Chart} chart 
 * @param {number[]} dataPoints 
 */
function pushDataToChart(chart, dataPoints) {
  if (!chart) return;
  const datasets = chart.data.datasets;
  dataPoints.forEach((value, i) => {
    if (datasets[i]) {
      datasets[i].data.push(value);
      if (datasets[i].data.length > CONSTANTS.MAX_CHART_POINTS) {
        datasets[i].data.shift();
      }
    }
  });
  chart.update('none');
}

/**
 * 将遥测数据馈送到所有图表
 * @param  {...number} data - 9个数据点
 */
export function feedChartsData(...data) {
  pushDataToChart(state.charts.chart1, data.slice(0, 3));
  pushDataToChart(state.charts.chart2, data.slice(3, 6));
  pushDataToChart(state.charts.chart3, data.slice(6, 9));
}